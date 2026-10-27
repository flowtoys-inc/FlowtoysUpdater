/*
  ==============================================================================

    PropConnectScreen.cpp
    Created: 4 Sep 2018 12:44:25pm
    Author:  Ben

  ==============================================================================
*/

#include "PropConnectScreen.h"
#include "FirmwareManager.h"

PropConnectScreen::PropConnectScreen() :
	AppScreen("Connect Props", CONNECT),
	flashBT("next step"),
	helpBT("helpBT"),
	multipleRevisionsDetected(false)
{
	PropManager::getInstance()->addAsyncManagerListener(this);
	addAndMakeVisible(&flashBT);
	flashBT.addListener(this);
	flashBT.setEnabled(false);

	helpBT.setColour(helpBT.textColourId, Colours::lightblue);
	helpBT.setText("need help? click here.", dontSendNotification);
	helpBT.setJustificationType(Justification::centred);
	addAndMakeVisible(&helpBT);
	helpBT.setMouseCursor(MouseCursor::PointingHandCursor);
	helpBT.addMouseListener(this, false);


	resetBTLink.setColour(resetBTLink.textColourId, Colours::purple.brighter(.7f));
	resetBTLink.setText("click here for reset instructions.", dontSendNotification);
	resetBTLink.setJustificationType(Justification::centred);
	addAndMakeVisible(&resetBTLink);
	resetBTLink.setMouseCursor(MouseCursor::PointingHandCursor);
	resetBTLink.addMouseListener(this, false);
}

PropConnectScreen::~PropConnectScreen()
{
	if (PropManager::getInstanceWithoutCreating() != nullptr) PropManager::getInstance()->removeAsyncManagerListener(this);
}

void PropConnectScreen::paint(Graphics &g)
{
	Rectangle<int> r = getLocalBounds();
	Rectangle<int> tr = r.removeFromTop(10);
	Rectangle<int> fr = r.removeFromBottom(200);

	int numProps = PropManager::getInstance()->props.size();
	String s = "";

	if (PropManager::getInstance()->selectedType == PropType::CAPSULE)
	{
		s = "Connect your capsule(s) now \
\n\n- when connected you should see it in the updater and it should light up blue. \
\n\n- older capsules may not turn blue when connected, but should show up in the updater. \
\n\n- If your capsule does NOT connect to the updater, try a RESET while plugged in and turned on.Click on the button below for reset instructions. \
\n\n- capsule lights do not reliably auto-connect to the updater, you may need to do a reset, \
\nor you can \"save\" P5M10 to connect. (navigate to P5M10 (bright red) and hold for about 7 sec. \
\nuntil the light is back on, then let go, it should connect)";

		if (numProps == 0)
		{
			s += "\n\nupdating multiple capsules ? \
\nyou can use a USB hub to update them all at once!\
\nconnect them all via USB, then click the next step button below. \
\nNOTE: saved settingsand favorites are retained after update, \
\ngrouping might be lost depending on version, \
\nand favorites might look different due to difference in mode / adjust.";
		}
	}
	else
	{
		s = "Connect your vision prop(s) now";

		if (numProps == 0)
		{
			s += "\n\nupdating multiple vision props? \
\nif they are the same props/use the same firmware, you can use a USB hub to update them all at once. \
\nconnect them all via USB, then click the next step button below";
		}
	}


	if (numProps > 0)
	{
		s += "\n\n-------   CONNECTED PROPS   ----------\n\n";
		if (multipleRevisionsDetected)
		{
			s += "different hardware revisions have been detected in the connected props.\nplease only connect props of the same revision.\n\n";
		}
		else
		{
			s += String(numProps) + " " + displayNames[(int)(PropManager::getInstance()->selectedType)].toLowerCase() + " connected:\n";
		}
	}


	int index = 0;
	for (auto &p : PropManager::getInstance()->props)
	{
		if (index > 0) s += "\n";
		s += "[" + String(index) + "] " + p->infos;
		index++;
	}


	g.setColour(Colours::lightgrey);

	/*
	g.drawFittedText("firmware to be uploaded:\n" + FirmwareManager::getInstance()->selectedFirmware->infos, tr, Justification::centred, 2);
	*/

	g.drawFittedText(s, r, Justification::centredTop, numProps + 1);
}

void PropConnectScreen::resized()
{
	Rectangle<int> r = getLocalBounds();
	Rectangle<int> fr = r.removeFromBottom(100);

	helpBT.setBounds(fr.removeFromTop(20));
	if (resetBTLink.isVisible())
	{
		resetBTLink.setBounds(fr.removeFromTop(20));
	}
	flashBT.setBounds(fr.withSizeKeepingCentre(100, 40));
}

void PropConnectScreen::reset()
{
	PropManager::getInstance()->clear();


	PropManager::getInstance()->checkProps();
}

void PropConnectScreen::checkProps()
{
	uint16_t hwRev = 0;
	multipleRevisionsDetected = false;

	if (PropManager::getInstance()->selectedType == PropType::CLUB) // only check vision
	{
		for (auto &p : PropManager::getInstance()->props)
		{
			if (hwRev == 0) hwRev = p->hw_rev;
			if (hwRev != p->hw_rev)
			{
				DBG("Revision different " << (int)hwRev << " / " << (int)p->hw_rev);
				multipleRevisionsDetected = true;
				break;
			}
		}
	}

	resetBTLink.setVisible(PropManager::getInstance()->props.size() == 0);
	flashBT.setEnabled(PropManager::getInstance()->props.size() > 0 && !multipleRevisionsDetected);

	resized();
}

void PropConnectScreen::mouseDown(const MouseEvent &e)
{
	AppScreen::mouseDown(e);

	if (e.eventComponent == &helpBT)
	{
		String link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000216768-flowos-firmware-updater-troubleshooting";
		if (PropManager::getInstance()->selectedType == PropType::CLUB)
			link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000213554-how-to-update-your-vision-prop-firmware";
		else
			link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000213534-how-to-update-your-capsule-2-0-firmware";
		URL(link).launchInDefaultBrowser();
	}
	else if (e.eventComponent == &resetBTLink)
	{
		String link = "";
		if (PropManager::getInstance()->selectedType == PropType::CLUB)
			link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000202797-vision-clubs-i-m-having-trouble-grouping";
		else
			link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000232163-how-to-reset-a-capsule-2-0-light";
		URL(link).launchInDefaultBrowser();
	}
}

void PropConnectScreen::newMessage(const PropManager::PropManagerEvent &e)
{
	switch (e.type)
	{
		case PropManager::PropManagerEvent::PROPS_CHANGED:
			checkProps();
			repaint();
			break;

		default:
			break;
	}
}

void PropConnectScreen::buttonClicked(Button *b)
{
	if (b == &flashBT)
	{
		screenListeners.call(&ScreenListener::screenFinish, this);
	}
}
