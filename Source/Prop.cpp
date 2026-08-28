/*
  ==============================================================================

    Prop.cpp
    Created: 4 Sep 2018 12:45:57pm
    Author:  Ben

  ==============================================================================
*/

#include "Prop.h"

#pragma warning(disable:4189 4244)



Prop::Prop(StringRef productString, StringRef serial, hid_device * device, PropType type, int vid, int pid, const wchar_t * serialNumber) :
	Thread("Prop"),
	productString(productString),
	type(type),
	serial(serial),
	dVid(vid),
	dPid(pid),
    device(device),
    deviceAckStatus(0),
    dSN(serialNumber),
	deviceStatus(NotSet),
	progression(0),
	infos("[Not set]"),
	numNoResponses(0),
	isFlashing(false),
    queuedNotifier(50)
{
	
    if(!this->productString.toLowerCase().contains("bootloader"))
    {
        DBG("App mode, reset to bootloader");
        sendAppReset(Subject::Bootloader);
        sendGetStatus();
        return;
    }
    
	sendGetVersion(Bootloader);
	sendGetVersion(App);


	infos = "firmware: " + fwVersion + ", created: " + fw_date + ", hardware ID: " + String(serialNumber);
    
    
}

Prop::~Prop()
{
	//Reads now time out (see readResponse), so the thread exits promptly on
	//the signal; only close the handle once the thread is really gone (#10).
	stopThread(3000);
	hid_close(device);
}


void Prop::sendReset()
{
	MemoryOutputStream s;
	s.writeInt(Command::Reset);
	sendPacket(s);
}

void Prop::sendAppReset(Subject subject)
{
	MemoryOutputStream s;
	s.writeInt(Command::AppReset);
	s.writeInt(subject);
	sendPacket(s);
}

void Prop::sendUpdate(int _totalBytesToSend)
{
	//DBG("Send update, vid " << String::toHexString(vid) << ", pid " << String::toHexString(pid));
	MemoryOutputStream s;
	s.writeInt(Command::Update);
	s.writeInt(_totalBytesToSend);
	s.writeInt(0); //CRC = 0
	s.writeShort(vid);
	s.writeShort(pid);
	s.writeShort(hw_rev);
	sendPacket(s);
}

void Prop::sendData(const char* data, int length)
{
	MemoryOutputStream s;
	s.writeInt(Command::Data);
	s.write(data, length);
	sendPacket(s);
}

void Prop::sendGetStatus()
{
	MemoryOutputStream s;
	s.writeInt(Command::GetStatus);
	sendPacket(s);
	std::unique_ptr<MemoryInputStream> result(readResponse());

	if (result != nullptr)
	{
		numNoResponses = 0; //only *consecutive* misses count towards a disconnect

		uint8_t cmd = result->readByte();
		for (int i = 0; i < 3; i++) result->readByte(); //2 useless bytes
		deviceAckStatus = result->readInt();
		deviceStatus = (Status)result->readByte();

		statusRawMessage = result->readString();

		DBG("Status response : " << cmd << " / " << deviceAckStatus << " / " << deviceStatus << " : " << statusRawMessage);
	}
	else
	{
		if (isFlashing)
		{
			numNoResponses++;
			if (numNoResponses >= 20) deviceStatus = Error;
		}
		
		DBG("NO RESPONSE ! ");
	}
}

void Prop::sendGetVersion(Subject subject)
{
	MemoryOutputStream s;
	s.writeInt(Command::GetVersion);
	s.writeInt(subject);
	sendPacket(s);

	std::unique_ptr<MemoryInputStream> result(readResponse());

	if (result != nullptr)
	{
		jassert(result->getDataSize() == 64);

		uint8_t cmd = result->readByte();

		if (subject == App) appActive = result->readByte() == 1;
		else bootloaderActive = result->readByte() == 1;

		for (int i = 0; i < 2; i++) result->readByte(); //2 bytes padding

		int _vid = result->readShort();
		int _pid = result->readShort();
		int _hw_rev = result->readShort();
		int _fw_rev = result->readShort();
		int64 _fw_date = result->readInt();
		char git_rev[8];
		result->read(git_rev, 8);

		String _gitRevString = "unknown";
		if (CharPointer_ASCII::isValidString(git_rev, std::numeric_limits<int>::max()))
		{
			_gitRevString= String(git_rev);
		}
		


		if (subject == Bootloader)
		{
			vid = _vid;
			pid = _pid;
			hw_rev = _hw_rev;
			gitRevString = _gitRevString;
		} else
		{
			fw_date = Time((int64)(_fw_date * 1000)).toString(true, false);
			fw_rev = _fw_rev;
			fwVersion = String(fw_rev >> 8) + "." + String(fw_rev & 0xff);
		}

		DBG("Date bootloader " << (int64)(_fw_date * 1000) << " : " << fw_date);


		char fw_ident[20];
		result->read(fw_ident, 20);
		String fwString = "unknown";
		if (CharPointer_ASCII::isValidString(fw_ident, std::numeric_limits<int>::max()))
		{
			fwString=  String(fw_ident);
		}

		if (subject == App) fwIdentString = fwString;

		DBG((subject == App ? "App" : "Bootloader") << " version response : cmd = " << (int)cmd << ", appActive = " << (int)appActive << ", bootloader active = " << (int)bootloaderActive << ", vid = " << String::toHexString(vid) << ", pid = " << String::toHexString(pid) << ", hw_rev = " << (int)hw_rev << ", fw_rev = " << fwVersion << ", date = " << fw_date << ", git rev = " << gitRevString << ", fwIdent = " << fwString);
	} else
	{
		DBG("Result is null !");
	}
}


void Prop::sendPacket(MemoryOutputStream & stream, bool prependReportByte)
{
	if (device == nullptr)
	{
		DBG("Device null !");
		return;
	}

	MemoryOutputStream data;
	if (prependReportByte) data.writeByte(0);
	data.write(stream.getData(), stream.getDataSize());
	while (data.getDataSize() < PACKET_SIZE + (prependReportByte ? 1 : 0)) data.writeByte(0); //prepend report id


																							  //const unsigned char * d = (const unsigned char *)data.getData();
																							  //for (int i = 0; i < PACKET_SIZE; i++) DBG("Sending " << i << " : " << (int)d[i]);

	hid_write(device, (const unsigned char *)data.getData(), data.getDataSize());
	//DBG("Send packet " << bytesWritten << " bytes sent");
}

MemoryInputStream * Prop::readResponse()
{
	if (device == nullptr)
	{
		DBG("Device null !");
		return nullptr;
	}

	unsigned char response[PACKET_SIZE];
	//Bounded read: a silent device must not hang the calling thread forever
	//(the old blocking hid_read did exactly that on macOS, #10). The 1s
	//timeout combined with the 20-consecutive-misses rule in sendGetStatus
	//gives a device ~20s of total silence before it is declared gone.
	int numRead = hid_read_timeout(device, response, PACKET_SIZE, 1000);

	//DBG("Read " << numRead << " bytes read");

	//for (int i = 0; i < PACKET_SIZE; i++) DBG(i << " : " << (int)response[i]);

	if (numRead > 0)
	{
		MemoryInputStream * result = new MemoryInputStream(response, numRead, true);
		return result;
	} else
	{
		return nullptr;
	}
}

void Prop::flash(MemoryBlock * _dataBlock, int _totalBytesToSend)
{
	stopThread(3000);

	this->dataBlock= _dataBlock;
	this->totalBytesToSend = _totalBytesToSend;

	startThread();
}

void Prop::run()
{
	const int bufferSize = 64; //fixed for bootloader

	numNoResponses = 0;
	isFlashing = true;
	setProgression(0);

	sendGetStatus();

	sendGetStatus();

	if (deviceStatus != Idle)
	{
		sendReset();

		//Bounded: a device that never reaches Idle used to spin here forever (#10).
		while (deviceStatus != Idle && !threadShouldExit())
		{
			sleep(10);
			sendGetStatus();

			if (deviceStatus == Error)
			{
				DBG("ERROR while waiting for Idle: " << statusRawMessage);
				progression = 0;
				isFlashing = false;
				queuedNotifier.addMessage(new PropEvent(this, PropEvent::FLASH_ERROR, progression));
				return;
			}
		}

		if (threadShouldExit()) { isFlashing = false; return; }
	}

	sendUpdate(totalBytesToSend);
	sleep(10);


	DBG("Erasing...");

	int sizeToErase = type == CAPSULE ? 51200 : 113664; //Size of the data to erase varies depending on the prop type

	int index = 0;

	while (!threadShouldExit())
	{
		sendGetStatus();
		sleep(1);

		if (deviceStatus == Error)
		{
			DBG("ERROR ! " << statusRawMessage);
			//flashState->setValueWithData(ERROR);
			progression = 0;
			isFlashing = false;
			queuedNotifier.addMessage(new PropEvent(this, PropEvent::FLASH_ERROR, progression));
			return;
		}
		else if (deviceStatus == EraseBusy)
		{
			//Erasing..
			DBG("Erasing...");
		}
		else if (deviceStatus == ProgramIdle)
		{
			DBG("Erase complete.");
			break;
		}

		DBG(deviceAckStatus);
		//No MessageManagerLock here: the notifier already delivers to the
		//message thread, and holding the lock while the message thread waits
		//in stopThread() was a deadlock (#10).
		if (deviceAckStatus > 0)
		{
			float newProgression = jmin(deviceAckStatus * .5f / sizeToErase, .5f);
			if (newProgression - progression >= 0.005f) setProgression(newProgression);
		}
		index++;
	}

	DBG("Flashing...");

	int currentPacket = 0;
	int currentDataOffset = 0;


	while (!threadShouldExit())
	{
		sendGetStatus();
		if (deviceStatus == Error)
		{
			DBG("ERROR ! " << statusRawMessage);
			//flashState->setValueWithData(ERROR);
			progression = 0;
			isFlashing = false;
			queuedNotifier.addMessage(new PropEvent(this, PropEvent::FLASH_ERROR, progression)); 
			//return false;
			return;

		}
		else if (deviceStatus == ProgramIdle && deviceAckStatus < totalBytesToSend)
		{
			//DBG("Sending with packet #" << currentPacket<<", offset : " << currentDataOffset << " (ackStatus : " << deviceAckStatus << ")");
			char data[DATA_PACKET_MAX_LENGTH];
			memset(data, 0, sizeof(char) * DATA_PACKET_MAX_LENGTH);
			dataBlock->copyTo(data, deviceAckStatus, DATA_PACKET_MAX_LENGTH);
			sendData(data, DATA_PACKET_MAX_LENGTH);

			currentDataOffset = deviceAckStatus;
			currentPacket++;

		}
		else if (deviceStatus == ProgramDone)
		{
			//Out of the loop
			break;
		}

		if (currentPacket % 10 == 0)
		{
			DBG("Progression : " << progression);
			setProgression(.5f + deviceAckStatus * .5f / totalBytesToSend);
		}
	}

	progression = 1;
	isFlashing = false;

	DBG("Flashing done !");
	queuedNotifier.addMessage(new PropEvent(this, PropEvent::FLASH_SUCCESS, progression));
	sendAppReset(Subject::App);
	
}

void Prop::setProgression(float value)
{
	progression = value;
	queuedNotifier.addMessage(new PropEvent(this, PropEvent::FLASHING_PROGRESS, progression));
}

