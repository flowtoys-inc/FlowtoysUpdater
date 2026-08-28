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
	startThread();
}

void FirmwareManager::clearFirmwares()
{
	DBG("Clear firmwares");
	Array<File> files = firmwareFolder.findChildFiles(File::TypesOfFileToFind::findFiles, false, "*.fwimg");
	for (auto &f : files) f.deleteFile();
}

void FirmwareManager::loadFirmwares()
{
	firmwares.clear();

	DBG("Loading firmwares");
	Array<File> files = firmwareFolder.findChildFiles(File::TypesOfFileToFind::findFiles, false, "*.fwimg");
    
    files.sort();
    
	for (auto &f : files)
	{
		Firmware * fw = getFirmwareForFile(f);
		if(fw != nullptr) firmwares.add(fw);
	}

	firmwares.sort(comparator, true);

	DBG(firmwares.size() << " loaded.");
	queuedNotifier.addMessage(new FirmwareManagerEvent(FirmwareManagerEvent::FIRMWARE_LOADED));
}

Firmware * FirmwareManager::getFirmwareForFile(File f)
{
	if (f.getSize() == 0)
	{
		DBG("Wrong file size, removing file");
		f.deleteFile();
		return nullptr;
	}

	ZipFile zip(f);
	const ZipFile::ZipEntry * meta = zip.getEntry("meta");
	const ZipFile::ZipEntry * data = zip.getEntry("data");


	//data
	std::unique_ptr<InputStream> dataStream(zip.createStreamForEntry(*data));
	if (dataStream == nullptr)
	{
		DBG("INVALID FILE, DELETING");
		f.deleteFile(); 
		return nullptr;
	}

	int numBytesToSend = (int)dataStream->getTotalLength();
	float numDataPackets = (float)(ceilf(numBytesToSend*1.0f / DATA_PACKET_MAX_LENGTH));
	int totalBytesToSend = (int)(numDataPackets * DATA_PACKET_MAX_LENGTH);
	//DBG("File size : " << numBytesToSend << ", split into " << numDataPackets << " =  " << totalBytesToSend << " total bytes");
	MemoryBlock fwData;
	dataStream->readIntoMemoryBlock(fwData);


	std::unique_ptr<InputStream> metaStream(zip.createStreamForEntry(*meta));
	var fwMeta = JSON::fromString(metaStream->readEntireStreamAsString());

	if (!fwMeta.isObject())
	{
		DBG("Problem with meta here");
		f.deleteFile(); 
		return nullptr;
	}

	//DBG("Got firmware : " << JSON::toString(fwMeta));

	int targetVID = (int)fwMeta.getProperty("usb_vid", 0);
	int targetPID = (int)fwMeta.getProperty("usb_pid", 0);
	uint16 fwRev = (uint16)(int)fwMeta.getProperty("fw_rev", 0);
	int hwRev = (int)fwMeta.getProperty("hw_rev", 0);
	String targetVersion = String(fwRev >> 8) + "." + String(fwRev & 0xff);
	String fwDate = Time((int64)((int64)fwMeta.getDynamicObject()->getProperty("fw_date")) * 1000).toString(true, false);
	String gitRev = fwMeta.getProperty("git_rev", "[not set]");
	String fwIdent = fwMeta.getProperty("fw_ident", "[not set]");

	String fwInfos = fwIdent + ", version " + targetVersion + " (" + fwDate + ")";
	Firmware * fw = new Firmware(fwData, totalBytesToSend, fwMeta, f.getFileNameWithoutExtension(), targetVersion, targetVersion.getFloatValue(), hwRev, targetPID, targetVID);
	
	//DBG("Firmware : " << String::toHexString(fw->hwRev) << " : " << Firmware::getHwRevNameforHwRev(fw->hwRev));

	for (int i = 0; i < TYPE_MAX; i++)
	{
		if (productIds[i] == targetPID) fw->type = (PropType)i;
	}



	return fw;
}

bool FirmwareManager::setLocalFirmware(File f, PropType expectedType)
{
	Firmware * fw = getFirmwareForFile(f);
	if (fw == nullptr)
	{
		delete fw;
		return false;
	}
	if (fw->type != expectedType)
	{
		delete fw;
		return false;
	}

	localFirmware.reset(fw);
	return fw != nullptr;
}

float FirmwareManager::getFirmwaresProgress()
{
	float p = 0;
	for (auto& fp : firmwareProgress) p += fp;
	p /= firmwareProgress.size();
	return p;
}

bool FirmwareManager::firmwaresAreLoaded()
{
	return firmwares.size() > 0;
}

Array<Firmware *> FirmwareManager::getFirmwaresForType(PropType type, int hardwareRevision)
{
	Array<Firmware *> result;
	if (type == NOTSET) return result;

	int targetPID = productIds[type];
	for (auto &f : firmwares)
	{
		if (f->pid == targetPID && (hardwareRevision == -1 || hardwareRevision == f->version)) result.add(f);
	}

	if (localFirmware != nullptr && localFirmware->type == type) result.add(localFirmware.get());

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
#if JUCE_WINDOWS
	if (statusCode != 200)
	{
		DBG("Failed to connect, status code = " << String(statusCode));
		errored = true;
		loadFirmwares();
		return;
	}
#endif

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
						std::unique_ptr<Firmware> fw(getFirmwareForFile(f));
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
	float p = downloaded / total;
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
	startThread();
}
