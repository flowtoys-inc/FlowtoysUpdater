/*
  ==============================================================================

    PropManager.h
    Created: 4 Sep 2018 1:36:56pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "Prop.h"

class Firmware;

class PropManager :
	public Timer,
	public Prop::AsyncListener
{
public:
	juce_DeclareSingleton(PropManager, true)

	PropManager();
	~PropManager();
	
	void checkProps();

	OwnedArray<Prop> props;

	Array<float> flashProgresses;
	Array<bool> flashSuccess;
	Array<bool> processedFlashes;

	//Settings from user choices
	//bool shouldCheck;
	PropType selectedType;
	int selectedRevision;

	bool isFlashing;

	//Holds the firmware being flashed for the whole flash duration, so a
	//background firmware-list refresh can never free it under the flash
	//threads (#9).
	std::shared_ptr<Firmware> flashingFirmware;

	void setSelectedType(PropType t);

    void clear();
	void reset();

	Prop * getItemWithSerial(StringRef serial);
	Prop * getItemWithHidDevice(hid_device * device);

	Prop * openDevice(hid_device_info * deviceInfo);

	PropType getTypeForProductID(int productID);

	void newMessage(const Prop::PropEvent &e) override;

	void computeProgression();

	void resetPropToBootloader(hid_device_info * deviceInfo);

	void flash();


	// Inherited via Timer
	virtual void timerCallback() override;

	class PropManagerEvent
	{
	public:
		enum Type { PROPS_CHANGED, FLASHING_PROGRESS, FLASHING_FINISHED };
		PropManagerEvent(Type type) : type(type) {}
		PropManagerEvent(Type type, float progress, int numFlashed = 0, int numErrored = 0) : type(type), progress(progress), numFlashed(numFlashed), numErrored(numErrored) {}

		Type type;
		float progress;
		int numFlashed;
		int numErrored;
	};

	QueuedNotifier<PropManagerEvent> queuedNotifier;
	typedef QueuedNotifier<PropManagerEvent>::Listener AsyncListener;
	void addAsyncManagerListener(AsyncListener* newListener) { queuedNotifier.addListener(newListener); }
	void addAsyncCoalescedManagerListener(AsyncListener* newListener) { queuedNotifier.addAsyncCoalescedListener(newListener); }
	void removeAsyncManagerListener(AsyncListener* listener) { queuedNotifier.removeListener(listener); }
};
