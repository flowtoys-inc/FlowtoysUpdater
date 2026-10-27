/*
  ==============================================================================

    Prop.h
    Created: 4 Sep 2018 12:45:57pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "hidapi.h"
#include "PropConstants.h"
#include "QueuedNotifier.h"

class Prop : public Thread
{
public:
	Prop(StringRef productString, StringRef serial = "", hid_device* device = nullptr, PropType type = PropType::CAPSULE, int vid = 0, int pid = 0, const wchar_t* serialNumber = 0);
	~Prop();

	//enum FlashState { READY, FLASHING, SUCCESS, ERROR };
	//EnumParameter* flashState;

	String productString;

	//DeviceInfo from hidapi
	int dVid;
	int dPid;
	const wchar_t* dSN;

	//Commands and status values
	enum Command
	{
		GetStatus = 0x00,
		Reset = 0x02,
		AppReset = 0xff,
		Update = 0x01,
		Data = 0x80,
		GetVersion = 0x10
	};

	enum Subject
	{
		Bootloader = 1,
		App = 2
	};

	enum Status
	{
		NotSet = 0,
		Idle = 2,
		EraseBusy = 3,
		ProgramIdle = 4,
		ProgramBusy = 5,
		ProgramDone = 6,
		Error = 10
	};

	PropType type;
	String serial;
	hid_device* device;

	//Infos
	uint16_t vid;
	uint16_t pid;
	uint16_t hw_rev;
	uint16_t fw_rev;
	String fw_date;
	String gitRevString;
	String fwIdentString;
	String fwVersion;

	Status deviceStatus;
	int deviceAckStatus;
	String statusRawMessage;


	//Parameters
	String infos;
	float progression;
	bool isFlashing;

	bool bootloaderActive;
	bool appActive;

	//flashing
	MemoryBlock* dataBlock;
	int totalBytesToSend;
	int numNoResponses; //check disconnected

	// Threaded call from CapsuleEngine
	void flash(MemoryBlock* b, int totalBytesToSend);

	void run() override;

	void setProgression(float value);

	//Flashing routine
	void sendReset();
	void sendAppReset(Subject subject);
	void sendUpdate(int _totalBytesToSend);
	void sendData(const char* data, int length);
	void sendGetStatus();
	void sendGetVersion(Subject subject);

	void sendPacket(MemoryOutputStream& data, bool prependReportByte = true);
	MemoryInputStream* readResponse();

	class PropEvent
	{
	public:
		enum Type
		{
			FLASHING_PROGRESS,
			FLASH_ERROR,
			FLASH_SUCCESS
		};
		PropEvent(Prop* prop, Type type) :
			prop(prop),
			type(type)
		{
		}
		PropEvent(Prop* prop, Type type, float progress) :
			prop(prop),
			type(type),
			progress(progress)
		{
		}

		Prop* prop;
		Type type;
		float progress;
	};

	QueuedNotifier<PropEvent> queuedNotifier;
	typedef QueuedNotifier<PropEvent>::Listener AsyncListener;
	void addAsyncCapsuleListener(AsyncListener* newListener) { queuedNotifier.addListener(newListener); }
	void addAsyncCoalescedCapsuleListener(AsyncListener* newListener) { queuedNotifier.addAsyncCoalescedListener(newListener); }
	void removeAsyncCapsuleListener(AsyncListener* listener) { queuedNotifier.removeListener(listener); }
};
