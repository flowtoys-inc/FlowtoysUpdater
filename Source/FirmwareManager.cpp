/*
  ==============================================================================

    FirmwareManager.cpp
    Created: 4 Sep 2018 12:46:38pm
    Author:  Ben

  ==============================================================================
*/

#include "FirmwareManager.h"

juce_ImplementSingleton(FirmwareManager)

	FirmwareManager::FirmwareManager() :
	Thread("Firmwares"),
	selectedFirmware(nullptr),
	errored(false),
	queuedNotifier(50)

{
	firmwareFolder = File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getChildFile("FlowtoysFirmwares");
	if (!firmwareFolder.exists()) firmwareFolder.createDirectory();

	startThread();
	startTimer(1000 * 3600); //check every hour
}

FirmwareManager::~FirmwareManager()
{
	signalThreadShouldExit();
	waitForThreadToExit(3000);
}

void FirmwareManager::initLoad()
{
	if (!isThreadRunning()) startThread();
}

void FirmwareManager::clearFirmwares()
{
	DBG("Clear firmwares");
	Array<File> files = firmwareFolder.findChildFiles(File::TypesOfFileToFind::findFiles, false, "*.fwimg");
	for (auto &f : files)
		f.deleteFile();
}

void FirmwareManager::loadFirmwares()
{
	DBG("Loading firmwares");
	Array<File> files = firmwareFolder.findChildFiles(File::TypesOfFileToFind::findFiles, false, "*.fwimg");

	files.sort();

	Array<std::shared_ptr<Firmware>> newFirmwares;
	for (auto &f : files)
	{
		std::shared_ptr<Firmware> fw = getFirmwareForFile(f);
		if (fw != nullptr) newFirmwares.add(fw);
	}

	newFirmwares.sort(comparator, true);

	{
		const ScopedLock sl(firmwaresLock);
		firmwares.swapWith(newFirmwares);
	}

	DBG(firmwares.size() << " loaded.");
	queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOADED));
}

std::shared_ptr<Firmware> FirmwareManager::getFirmwareForFile(File f)
{
	if (f.getSize() == 0)
	{
		DBG("Wrong file size, removing file");
		f.deleteFile();
		return nullptr;
	}

	FileInputStream fis(f);
	std::unique_ptr<FirmwareImage> img = fis.openedOk() ? FirmwareImage::parse(fis, DATA_PACKET_MAX_LENGTH) : nullptr;

	if (img == nullptr)
	{
		DBG("INVALID FILE, DELETING");
		f.deleteFile();
		return nullptr;
	}

	std::shared_ptr<Firmware> fw = std::make_shared<Firmware>(img->data, img->totalBytesToSend, img->meta, f.getFileNameWithoutExtension(), img->versionString, img->version, img->hwRev, img->pid, img->vid, img->fwRev);

	for (int i = 0; i < TYPE_MAX; i++)
	{
		if (productIds[i] == img->pid) fw->type = (PropType)i;
	}

	return fw;
}

bool FirmwareManager::setLocalFirmware(File f, PropType expectedType)
{
	std::shared_ptr<Firmware> fw = getFirmwareForFile(f);
	if (fw == nullptr) return false;
	if (fw->type != expectedType) return false;

	localFirmware = fw;
	return true;
}

float FirmwareManager::getFirmwaresProgress()
{
	if (firmwareProgress.isEmpty()) return 0;
	float p = 0;
	for (auto &fp : firmwareProgress)
		p += fp;
	p /= firmwareProgress.size();
	return p;
}

bool FirmwareManager::firmwaresAreLoaded()
{
	const ScopedLock sl(firmwaresLock);
	return firmwares.size() > 0;
}

Array<std::shared_ptr<Firmware>> FirmwareManager::getFirmwaresForType(PropType type, int hardwareRevision)
{
	Array<std::shared_ptr<Firmware>> result;
	if (type == NOTSET) return result;

	int targetPID = productIds[type];
	{
		const ScopedLock sl(firmwaresLock);
		for (auto &f : firmwares)
		{
			if (f->pid == targetPID && (hardwareRevision == -1 || hardwareRevision == f->hwRev)) result.add(f);
		}
	}

	if (localFirmware != nullptr && localFirmware->type == type) result.add(localFirmware);

	return result;
}

void FirmwareManager::run()
{
	errored = false;

	StringPairArray responseHeaders;
	int statusCode = 0;

	URL updateURL(FirmwareCatalog::indexUrl);
	std::unique_ptr<InputStream> stream(updateURL.createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inAddress)
																		.withConnectionTimeoutMs(2000)
																		.withResponseHeaders(&responseHeaders)
																		.withStatusCode(&statusCode)));
	//Enforced on every platform (was Windows-only, #22): a captive portal or
	//error page must not reach the JSON parser. A dead connection arrives
	//here as statusCode 0 and takes the same errored + local-cache path.
	if (statusCode != 200 || stream == nullptr)
	{
		DBG("Failed to connect, status code = " << String(statusCode));
		errored = true;
		queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOAD_ERROR));
		loadFirmwares();
		return;
	}

	auto catalog = FirmwareCatalog::parseIndex(stream->readEntireStreamAsString());
	if (!catalog.ok)
	{
		DBG("Error reading online firmware list (" << catalog.error << "), loading local firmwares");
		errored = true;
		loadFirmwares();
		return;
	}

	onlineFirmwares = catalog.entries.size();
	DBG("Got " << onlineFirmwares << " online firmwares");
	downloadedFirmwares = 0;
	firmwareProgress.clear();
	firmwareProgress.resize(onlineFirmwares);
	if (onlineFirmwares == 0) loadFirmwares();

	for (const auto &entry : catalog.entries)
	{
		File f = firmwareFolder.getChildFile(entry.file);

		//Names are immutable in the catalog, so a cached file with the
		//catalog's size and sha256 is current. Anything else is replaced.
		if (f.existsAsFile() && !fileMatchesEntry(f, entry))
		{
			DBG("Cached file doesn't match the catalog, re-downloading : " + f.getFileName());
			f.deleteFile();
		}

		if (f.existsAsFile())
		{
			firmwareProgress.set(downloadedFirmwares, 1);
			downloadedFirmwares++;
			if (downloadedFirmwares == onlineFirmwares) loadFirmwares();
			continue;
		}

		const String fURL = FirmwareCatalog::fileUrl(entry.file);
		DBG("Downloading " << fURL);
		std::unique_ptr<URL::DownloadTask> t = URL(fURL).downloadToFile(f, URL::DownloadTaskOptions().withListener(this));
		if (t == nullptr)
		{
			DBG("Download errored");
			firmwareProgress.set(downloadedFirmwares, 1);
			downloadedFirmwares++;
		}
		else
		{
			firmwareProgress.set(downloadedFirmwares, 0);
			taskEntries.add(entry);
			tasks.add(t.release());
		}
	}
}

bool FirmwareManager::fileMatchesEntry(const File &f, const FirmwareCatalog::Entry &e)
{
	if (f.getSize() != e.size) return false; //cheap check first
	return FirmwareCatalog::matches(e, f.getSize(), SHA256(f).toHexString());
}

void FirmwareManager::progress(URL::DownloadTask *t, int64 downloaded, int64 total)
{
	DBG("Progress !");
	float p = total > 0 ? (float)downloaded / (float)total : 0.0f;
	int index = tasks.indexOf(t);
	firmwareProgress.set(index, p);
	queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOAD_PROGRESS));
}

void FirmwareManager::finished(URL::DownloadTask *t, bool success)
{
	downloadedFirmwares++;

	//A download that doesn't match the catalog is never parsed or offered.
	const int taskIndex = tasks.indexOf(t);
	if (success && isPositiveAndBelow(taskIndex, taskEntries.size()))
	{
		const File f = t->getTargetLocation();
		if (!fileMatchesEntry(f, taskEntries[taskIndex]))
		{
			DBG("Downloaded file doesn't match the catalog : " + f.getFileName());
			f.deleteFile();
			success = false;
		}
	}

	if (success)
	{

		if (downloadedFirmwares == onlineFirmwares)
		{
			DBG("ALL firmware downloaded !");
			loadFirmwares();
		}
		else
		{
			int index = tasks.indexOf(t);
			firmwareProgress.set(index, 1);
			queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOAD_PROGRESS));
		}
	}
	else
	{
		DBG("Finished with errors");

		queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOAD_ERROR));
	}
}

void FirmwareManager::timerCallback()
{
	if (!isThreadRunning()) startThread();
}
