/*
  ==============================================================================

    PropManager.cpp
    Created: 4 Sep 2018 1:36:56pm
    Author:  Ben

  ==============================================================================
*/

#include "PropManager.h"
#include "hidapi.h"
#include "FirmwareManager.h"

juce_ImplementSingleton(PropManager)

PropManager::PropManager() :
	//shouldCheck(false),
	selectedType(NOTSET),
	selectedRevision(-1),
	isFlashing(false),
    queuedNotifier(100)
{
	startTimerHz(10);
}

PropManager::~PropManager()
{

}

void PropManager::clear()
{
    props.clear();
}

void PropManager::checkProps()
{
	if (isFlashing) return;
	//if (!shouldCheck) return;

	bool changed = false;
	Array<String> foundSerials;

	//for (int i = 0; i < TYPE_MAX; i++)
	//{
	hid_device_info * deviceInfos = hid_enumerate(flowtoysVID, (unsigned short)productIds[(int)selectedType]);
	hid_device_info * dInfo = deviceInfos;


	while (dInfo != nullptr)
	{
		Prop * p = getItemWithSerial(String(dInfo->serial_number));
        
        
		if (p != nullptr)
		{
			//already opened, do nothing;
        }else if(!String(dInfo->product_string).toLowerCase().contains("bootloader"))
        {
			PropType t = getTypeForProductID(dInfo->product_id);
			if (t == CLUB) //t
			{
				DBG("App mode and device is club, try put to bootloader");
				resetPropToBootloader(dInfo);

			} else
			{
				DBG("App mode and device is not club, not connecting");
			}
          
        }else
		{
			openDevice(dInfo);
			changed = true;
		}

		foundSerials.add(dInfo->serial_number);
		dInfo = dInfo->next;
	}

	hid_free_enumeration(deviceInfos);
	//}

	Array<Prop *> devicesToRemove;
	for (auto &d : props)
	{
		if (!foundSerials.contains(d->serial))
		{
			//if (d->flashState == Device::FLASHING) continue;
			DBG("Device removed : " << d->serial);
			devicesToRemove.add(d);
		}
	}

	for (auto &d : devicesToRemove)
	{
		props.removeObject(d);
		changed = true;
	}

	if (changed)
	{
		DBG("Prop Changed");
		queuedNotifier.addMessage(new PropManagerEvent(PropManagerEvent::PROPS_CHANGED));
	}
}


Prop * PropManager::getItemWithSerial(StringRef serial)
{
	for (auto &d : props) if (d->serial == serial) return d;
	return nullptr;
}

Prop * PropManager::getItemWithHidDevice(hid_device * device)
{

	for (auto &d : props) if (d->device == device) return d;
	return nullptr;
}

Prop * PropManager::openDevice(hid_device_info * deviceInfo)
{
	if (deviceInfo->vendor_id == 0 || deviceInfo->product_id == 0 || deviceInfo->serial_number == 0) return nullptr;

	hid_device * d = hid_open(deviceInfo->vendor_id, deviceInfo->product_id, deviceInfo->serial_number);
	if (d == NULL)
	{
		DBG("Device could not be opened " << deviceInfo->serial_number);
		return nullptr;
	}

	DBG("Device opened : " << deviceInfo->product_string << " (" << deviceInfo->manufacturer_string << ") " << deviceInfo->serial_number << " : " << String::toHexString(deviceInfo->vendor_id) << ", " << String::toHexString(deviceInfo->product_id) << ", " << deviceInfo->product_string);

	Prop * cd = new Prop(String(deviceInfo->product_string), String(deviceInfo->serial_number), d, getTypeForProductID(deviceInfo->product_id), (int)deviceInfo->vendor_id, (int)deviceInfo->product_id, deviceInfo->serial_number);
	props.add(cd);
	cd->addAsyncCapsuleListener(this);
	return cd;
}


void PropManager::setSelectedType(PropType t)
{
	props.clear(); 
	selectedType = t;
	//shouldCheck = true;
}

void PropManager::reset()
{
	props.clear();
	//shouldCheck = false;
}


void PropManager::timerCallback()
{
	checkProps();
}



//Constants

PropType PropManager::getTypeForProductID(int productID) {
	return propTypeForProductID(productID);
}

void PropManager::newMessage(const Prop::PropEvent & e)
{
	switch (e.type)
	{
	case Prop::PropEvent::FLASHING_PROGRESS:
		break;

	case Prop::PropEvent::FLASH_ERROR:
		if (!processedFlashes[props.indexOf(e.prop)])
		{
			flashSuccess.set(props.indexOf(e.prop), false);
			processedFlashes.set(props.indexOf(e.prop), true);
		}
		else
		{
			jassertfalse;
		}
		break;

	case Prop::PropEvent::FLASH_SUCCESS:
		flashSuccess.set(props.indexOf(e.prop), true);
		processedFlashes.set(props.indexOf(e.prop), true);
		break;
	}

	computeProgression();
}

void PropManager::computeProgression()
{
	if (props.isEmpty()) return; //avoid dividing by zero below

	float totalProgression = 0;
	int numSuccess = 0;
	int numErrorred = 0;
	for (int i = 0; i < props.size(); i++)
	{
		if (!props[i]->isFlashing)
		{
			totalProgression += 1;
			if (flashSuccess[i]) numSuccess++;
			else numErrorred++;
		}
		else
		{
			totalProgression += props[i]->progression;
		}
	}

	totalProgression /= props.size();

	DBG("Total progression " << totalProgression << " > success / error : " << numSuccess << " / " << numErrorred);
	queuedNotifier.addMessage(new PropManagerEvent(PropManagerEvent::FLASHING_PROGRESS, totalProgression, numSuccess, numErrorred));

	if (totalProgression == 1)
	{
		queuedNotifier.addMessage(new PropManagerEvent(PropManagerEvent::FLASHING_FINISHED, totalProgression, numSuccess, numErrorred));
		isFlashing = false;
		flashingFirmware.reset(); //flash done, firmware no longer needs pinning (#9)
	}
}

void PropManager::resetPropToBootloader(hid_device_info * deviceInfo)
{
	hid_device * d = hid_open(deviceInfo->vendor_id, deviceInfo->product_id, deviceInfo->serial_number);
	if (d != nullptr)
	{
		DBG("Reset device to bootloader");
		MemoryOutputStream data;
		data.writeByte(0);
		data.writeInt(Prop::Command::AppReset);
		data.writeInt(Prop::Subject::Bootloader);
		while (data.getDataSize() < PACKET_SIZE + 1) data.writeByte(0); //prepend report id

		hid_write(d, (const unsigned char *)data.getData(), data.getDataSize());
		hid_close(d); //this leaked a handle per 100ms poll tick (#10)
	} else
	{
		DBG("Could not open device");
	}
}

void PropManager::flash()
{
	if (props.isEmpty()) {
		DBG("Nothing to flash");
		return;
	}

	std::shared_ptr<Firmware> f = FirmwareManager::getInstance()->selectedFirmware;
	if (f == nullptr)
	{
		DBG("No firmware selected, not flashing");
		return;
	}

	flashingFirmware = f; //keep alive until FLASHING_FINISHED (#9)

	isFlashing = true;

	flashProgresses.clear();
	processedFlashes.clear();
	flashSuccess.clear();

	for (auto& d : props)
	{
		flashProgresses.add(0);
		flashSuccess.add(false);
		processedFlashes.add(false);

		DBG("Flashing device " << d->infos);
		d->flash(&flashingFirmware->data, flashingFirmware->totalBytesToSend);
	}

}

