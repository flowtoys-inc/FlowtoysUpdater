/*
  ==============================================================================

    UploadScreen.cpp
    Created: 4 Sep 2018 12:44:51pm
    Author:  Ben

  ==============================================================================
*/

#include "UploadScreen.h"

UploadScreen::UploadScreen() :
	AppScreen("Upload", UPLOAD),
	progression(0),
	numSuccess(0),
	numErrored(0)
{
	PropManager::getInstance()->addAsyncManagerListener(this);
}

UploadScreen::~UploadScreen()
{
}

void UploadScreen::reset()
{
	progression = 0;

	std::shared_ptr<Firmware> fw = FirmwareManager::getInstance()->selectedFirmware;
	if (fw == nullptr)
	{
		text = "No firmware selected.\nPlease go back and select a firmware.";
		repaint();
		return;
	}

	text = "updating with " + fw->infos + "\n- do not disconnect until complete -";
	PropManager::getInstance()->flash();
}

void UploadScreen::paint(Graphics & g)
{
	g.setColour(Colours::lightgrey);
	Rectangle<int> r = getLocalBounds().withSizeKeepingCentre(300, 40).translated(0, -60);
	String s = text + "\nprogression : " + String((int)(progression * 100)) + "%, " + String(numSuccess) + " success, " + String(numErrored) + " errored";
	g.drawFittedText(s, r, Justification::centred, 5);
	r.translate(0, 60);
	r.setSize(300, 20);
	g.fillRoundedRectangle(r.toFloat(), 2);
	Colour c = Colours::limegreen;
	if (numErrored > 0) c = progression < 1 ? Colours::orange : Colours::orangered;
	g.setColour(c);
	r.setWidth((int)(r.getWidth()*progression));
	g.fillRoundedRectangle(r.toFloat(), 2);
}

void UploadScreen::newMessage(const PropManager::PropManagerEvent & e)
{
	if (e.type == PropManager::PropManagerEvent::FLASHING_PROGRESS)
	{	
		numErrored = e.numErrored;
		numSuccess = e.numFlashed;
		progression = e.progress;

		repaint();
	} else if (e.type == PropManager::PropManagerEvent::FLASHING_FINISHED)
	{
		numErrored = e.numErrored;
		numSuccess = e.numFlashed;

		if (numErrored > 0)
		{
			text = "There were errors during uploading.\nPlease unplug, plug again your prop and try uploading again.";
			repaint();
		}
		else
		{
			screenListeners.call(&ScreenListener::screenFinish, this);
		}
		
	}
}
