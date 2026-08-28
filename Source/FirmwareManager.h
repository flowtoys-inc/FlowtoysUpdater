/*
  ==============================================================================

    FirmwareManager.h
    Created: 4 Sep 2018 12:46:38pm
    Author:  Ben

  ==============================================================================
*/

#pragma once
#include "JuceHeader.h"
#include "PropConstants.h"
#include "QueuedNotifier.h"
#include "FirmwareImage.h"
#include "VersionUtils.h"

class Firmware
{
public:
	Firmware(MemoryBlock data, int totalBytesToSend, var meta,  String infos, String versionString, float version, int hwRev, int pid, int vid) :
		data(data), totalBytesToSend(totalBytesToSend), infos(infos), meta(meta),  versionString(versionString), version(version), hwRev(hwRev), pid(pid), vid(vid)
	{}

	MemoryBlock data;
	int totalBytesToSend;


	String infos;
	String versionString;
	var meta;
	int hwRev;
	float version;
	int pid;
	int vid;
	PropType type = NOTSET;

	static String getHwRevNameforHwRev(int hwRev)
	{
		return FirmwareImage::getHwRevNameforHwRev(hwRev);
	}

	bool isHardwareCompatible(int hardwareRev)
	{
		return FirmwareImage::isHardwareRevCompatible(hwRev, hardwareRev, type == CAPSULE);
	}
};

class FirmwareComparator
{
public:
	FirmwareComparator() {}
	int compareElements(const std::shared_ptr<Firmware>& f1, const std::shared_ptr<Firmware>& f2)
	{
		//inverse order, we want the latest first. Compares dotted segments,
		//not the legacy float (where "1.10" sorted below "1.7").
		return VersionUtils::compareVersionsDescending(f1->versionString, f2->versionString);
	}
};

class FirmwareManager :
	public Thread,
	public URL::DownloadTask::Listener,
	public Timer
{
public:
	juce_DeclareSingleton(FirmwareManager,true)

	FirmwareManager();
	~FirmwareManager();

	FirmwareComparator comparator;
 
	const String remoteHost = "http://flow-toys.com/fusion/";
	File firmwareFolder;
    OwnedArray<URL::DownloadTask> tasks;

	//shared_ptr everywhere: the hourly refresh clears the list from the
	//background thread while the UI (and a running flash) may still hold
	//entries — shared ownership keeps them alive (#9).
	std::shared_ptr<Firmware> selectedFirmware;

	std::shared_ptr<Firmware> localFirmware;

	void initLoad();
	void clearFirmwares();
	void loadFirmwares();

	std::shared_ptr<Firmware> getFirmwareForFile(File f);
	bool setLocalFirmware(File f, PropType expectedType);

	Array<float> firmwareProgress;
	int downloadedFirmwares;
	int onlineFirmwares;
	bool errored;

	float getFirmwaresProgress();
	bool firmwaresAreLoaded();

	CriticalSection firmwaresLock; //guards firmwares (mutated on the bg thread, read on the message thread)
	Array<std::shared_ptr<Firmware>> firmwares;
	Array<std::shared_ptr<Firmware>> getFirmwaresForType(PropType type, int hardwareRevision);

	virtual void run() override;

	// Inherited via Listener
    virtual void progress (URL::DownloadTask* task, int64 bytesDownloaded, int64 totalLength) override;
    virtual void finished(URL::DownloadTask * task, bool success) override;


	void timerCallback() override;

	class FirmwareManagerEvent
	{
	public:
		enum Type { FIRMWARE_LOAD_PROGRESS, FIRMWARE_LOADED, FIRMWARE_LOAD_ERROR };
		FirmwareManagerEvent(Type type) : type(type) {}

		Type type;
	};

	QueuedNotifier<FirmwareManagerEvent> queuedNotifier;
	typedef QueuedNotifier<FirmwareManagerEvent>::Listener AsyncListener;
	void addAsyncManagerListener(AsyncListener* newListener) { queuedNotifier.addListener(newListener); }
	void addAsyncCoalescedManagerListener(AsyncListener* newListener) { queuedNotifier.addAsyncCoalescedListener(newListener); }
	void removeAsyncManagerListener(AsyncListener* listener) { queuedNotifier.removeListener(listener); }
};
