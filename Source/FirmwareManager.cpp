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
	startTimer(1000*3600); //check every hour
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
	for (auto &f : files) f.deleteFile();
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
		if(fw != nullptr) newFirmwares.add(fw);
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

	std::shared_ptr<Firmware> fw = std::make_shared<Firmware>(img->data, img->totalBytesToSend, img->meta, f.getFileNameWithoutExtension(), img->versionString, img->version, img->hwRev, img->pid, img->vid);

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
	for (auto& fp : firmwareProgress) p += fp;
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

	URL updateURL(remoteHost + "firmwares.php");
	std::unique_ptr<InputStream> stream(updateURL.createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inAddress)
		.withConnectionTimeoutMs(2000)
		.withResponseHeaders(&responseHeaders)
		.withStatusCode(&statusCode)));
	//Enforced on every platform (was Windows-only, #22): a captive portal or
	//error page must not reach the JSON parser. A dead connection arrives
	//here as statusCode 0 and takes the same errored + local-cache path.
	if (statusCode != 200)
	{
		DBG("Failed to connect, status code = " << String(statusCode));
		errored = true;
		loadFirmwares();
		return;
	}

	DBG("Firmware updater:: Status code " << statusCode);

	if (stream != nullptr)
	{
		String content = stream->readEntireStreamAsString(); 
		var data = JSON::parse(content);

		if (data.isObject())
		{
			bool shouldClear = data.getProperty("clear", false);
			if (shouldClear) clearFirmwares();

			var fileData = data.getProperty("files", var());
			if (fileData.isArray())
			{
				onlineFirmwares = fileData.size();
				DBG("Got " << onlineFirmwares << " online firmwares");
				downloadedFirmwares = 0;
				firmwareProgress.clear();
				firmwareProgress.resize(onlineFirmwares);
				for (int i = 0; i < onlineFirmwares; i++)
				{
					File f = firmwareFolder.getChildFile(fileData[i].toString());

					bool fileExistAndIsValid = false;
					if (f.existsAsFile() && f.getSize() > 0)
					{
						std::shared_ptr<Firmware> fw = getFirmwareForFile(f);
						if (fw != nullptr)
						{
							DBG("File already downloaded and valid : "+f.getFileName());
							fileExistAndIsValid = true;
						}
						else
						{
							DBG("File already there but not valid : " + f.getFileName());
							f.deleteFile();
						}
					}

					if (fileExistAndIsValid)
					{
						firmwareProgress.set(downloadedFirmwares, 1);
						downloadedFirmwares++;
						if (downloadedFirmwares == onlineFirmwares) loadFirmwares();
					}else
					{
                        String fURL = remoteHost + String("firmwares/") + URL::addEscapeChars(fileData[i].toString(),false); //add handling for spaces
                        
                        DBG("Downloading " << fURL);
						URL downloadURL(fURL);
                        std::unique_ptr<URL::DownloadTask> t = downloadURL.downloadToFile(f, URL::DownloadTaskOptions().withListener(this));
                        if(t == nullptr)
                        {
                            DBG("Download errored");
							firmwareProgress.set(downloadedFirmwares, 1);
							downloadedFirmwares++;
                        }else
                        {
							firmwareProgress.set(downloadedFirmwares, 0);
							tasks.add(t.release());
                        }
					}
				}
			}
		} else
		{
			DBG("Error reading online firmware list, loading local firmwares");
			errored = true;
			loadFirmwares();
		}
	} else
	{
		DBG("Couldn't access internet, loading local firmwares");
		errored = true;
		queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOAD_ERROR));
		loadFirmwares();
	}

}

void FirmwareManager::progress (URL::DownloadTask* t , int64 downloaded, int64 total)
{
    DBG("Progress !");
	float p = total > 0 ? (float)downloaded / (float)total : 0.0f;
	int index = tasks.indexOf(t);
	firmwareProgress.set(index, p);
	queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOAD_PROGRESS));
}

void FirmwareManager::finished(URL::DownloadTask * t, bool success)
{
	downloadedFirmwares++;

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
	}else
    {
        DBG("Finished with errors");

		queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOAD_ERROR));
    }
}

void FirmwareManager::timerCallback()
{
	if (!isThreadRunning()) startThread();
}
