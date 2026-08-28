/*
  ==============================================================================

	FirmwareChooserScreen.cpp
	Created: 4 Sep 2018 12:39:25pm
	Author:  Ben

  ==============================================================================
*/

#include "FirmwareChooserScreen.h"
#include "PropManager.h"

FirmwareChooserScreen::FirmwareChooserScreen() :
	AppScreen("Firmware", FW_CHOOSE),
	selectBT("upload"),
	chooseFileBT("Choose local file")
{
	addChildComponent(selectBT);
	addChildComponent(fwChooser);
	addAndMakeVisible(chooseFileBT);

	selectBT.addListener(this);
	chooseFileBT.addListener(this);
	fwChooser.addListener(this);

	helpBT.setColour(helpBT.textColourId, Colours::lightblue);
	helpBT.setText("need help? click here.", dontSendNotification);
	helpBT.setJustificationType(Justification::centred);
	addAndMakeVisible(&helpBT);
	helpBT.setMouseCursor(MouseCursor::PointingHandCursor);
	helpBT.addMouseListener(this, false);

	FirmwareManager::getInstance()->addAsyncManagerListener(this);

}

FirmwareChooserScreen::~FirmwareChooserScreen()
{
}

void FirmwareChooserScreen::updateVisibility()
{
	bool v = FirmwareManager::getInstance()->firmwaresAreLoaded();

	String noFWText = "No firmware available, are you connected to internet?";
	fwChooser.clear();
	fwChooser.setTextWhenNoChoicesAvailable(noFWText);
	
	fwList = FirmwareManager::getInstance()->getFirmwaresForType(PropManager::getInstance()->selectedType, -1);

	int targetHW = -1;
	if (PropManager::getInstance()->props.size() > 0)
	{
		targetHW = PropManager::getInstance()->props[0]->hw_rev;
	}

	
	int indexToSelect = -1;
	for (int i = 0; i < fwList.size(); i++)
	{
		bool isLocal = FirmwareManager::getInstance()->localFirmware == fwList[i];
		String label = fwList[i]->infos;
		if (isLocal) label += " (local)";
#if JUCE_DEBUG && JUCE_WINDOWS
		else if (targetHW == -1) {} //keep for testing
#endif
		else if (!fwList[i]->isHardwareCompatible(targetHW))
		{
			DBG("Not compatible hardware !");
			continue;
		}

		fwChooser.addItem(label, i + 1);
		if (isLocal) indexToSelect = i + 1;
		if (FirmwareManager::getInstance()->selectedFirmware != nullptr && fwList[i] == FirmwareManager::getInstance()->selectedFirmware) indexToSelect = i + 1;
	}

	fwChooser.setTextWhenNothingSelected(fwChooser.getNumItems() > 0 ? "select a firmware" : noFWText);


	selectBT.setVisible(v);
	fwChooser.setVisible(v);
	selectBT.setEnabled(fwChooser.getNumItems() > 0);
	if (indexToSelect != -1)
	{
		DBG("Index to select : " << indexToSelect);
		fwChooser.setSelectedId(indexToSelect);
	}
	else if (fwChooser.getNumItems() > 0)
	{
		fwChooser.setSelectedItemIndex(0);
	}

	repaint();
}

void FirmwareChooserScreen::paint(Graphics& g)
{
	bool loaded = FirmwareManager::getInstance()->firmwaresAreLoaded();
	float progress = FirmwareManager::getInstance()->getFirmwaresProgress();
	bool errored = FirmwareManager::getInstance()->errored;
	bool noOnlineFW = FirmwareManager::getInstance()->onlineFirmwares == 0;
	String s = "";
	if (!loaded && !errored)
	{
		if (noOnlineFW) s = "No firmwares available online or locally right now.";
		else  s = "Loading firmwares, please wait.... " + String((int)(progress * 100)) + "%";
	}

	else if (!loaded && errored) s = "There were errors while downloading online firmwares and no local firmwares availables.";
	else if (loaded && errored) s = "There were errors while downloading oneline firmwares, showing only local firmwares.";

	if (s.isNotEmpty()) g.setColour(Colours::orange);
	else
	{
		g.setColour(Colours::lightgrey);
		//s = "leave as \"auto update\" for the latest version\nor select your preferred firmware.";

		s += "SELECT YOUR FIRMWARE \
\n\nIf your updater has been open for awhile, restart to ensure the latest firmware versions have loaded";

		if (PropManager::getInstance()->selectedType == PropType::CLUB)
		{
			s += "\nselect your club type, or for all other props select \"spin(pixel count)\" or next higher pixel count available \
\n\n\nTO FIND THE CORRECT PIXEL COUNT FOR YOUR PROP \
\nturn on your prop + count the number of pixels down the length \
\nfor staves it is 1/2 the total pixels \
\nonly count 1 column of leds down the length \
\ndon't count every LED around the prop - there are 2-4 LEDs per pixel";
		}
	}

	g.drawFittedText(s, getLocalBounds().removeFromTop(150).reduced(40, 10), Justification::centred, 20, false);

}

void FirmwareChooserScreen::reset()
{
	if (!FirmwareManager::getInstance()->firmwaresAreLoaded() || FirmwareManager::getInstance()->errored) FirmwareManager::getInstance()->initLoad();

	updateVisibility();
}

void FirmwareChooserScreen::mouseDown(const MouseEvent& e)
{
	AppScreen::mouseDown(e);

	if (e.eventComponent == &helpBT)
	{
		String link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000216768-flowos-firmware-updater-troubleshooting";
		if (PropManager::getInstance()->selectedType == PropType::CLUB) link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000213554-how-to-update-your-vision-prop-firmware";
		else link = "https://flowtoys2.freshdesk.com/support/solutions/articles/6000213534-how-to-update-your-capsule-2-0-firmware";
		URL(link).launchInDefaultBrowser();
	}
}

void FirmwareChooserScreen::resized()
{
	Rectangle<int> r = getLocalBounds();
	Rectangle<int> br = r.removeFromBottom(200);
	selectBT.setBounds(br.withSizeKeepingCentre(100, 40).translated(0, -50));
	chooseFileBT.setBounds(br.withSizeKeepingCentre(120, 40));
	helpBT.setBounds(r.withSizeKeepingCentre(450, 30).translated(0,40));
	fwChooser.setBounds(r.withSizeKeepingCentre(450, 30).translated(0, 80));
}

void FirmwareChooserScreen::newMessage(const FirmwareManager::FirmwareManagerEvent&)
{
	updateVisibility();
}

void FirmwareChooserScreen::comboBoxChanged(ComboBox*)
{
}

void FirmwareChooserScreen::buttonClicked(Button* b)
{
	if (b == &selectBT)
	{
		int index = fwChooser.getSelectedId();
		FirmwareManager::getInstance()->selectedFirmware = (index <= 0 ? fwList[0] : fwList[index - 1]);
		screenListeners.call(&ScreenListener::screenFinish, this);
	}
	else if (b == &chooseFileBT)
	{
		FileChooser fc("Choose a firmware file", File(), "*.fwimg");
		bool result = fc.browseForFileToOpen();
		if (result)
		{
			File f = fc.getResult();
			bool fResult = FirmwareManager::getInstance()->setLocalFirmware(f, PropManager::getInstance()->selectedType);
			if (!fResult)
			{
				AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Bad file", "The file you chose is either corrupted or not a firmware for " + displayNames[PropManager::getInstance()->selectedType] + ".", "OK");
			}
			else
			{
				if (FirmwareManager::getInstance()->localFirmware != nullptr)
				{
					int targetHW = -1;
					if (PropManager::getInstance()->props.size() > 0) targetHW = PropManager::getInstance()->props[0]->hw_rev;

					std::shared_ptr<Firmware> fw = FirmwareManager::getInstance()->localFirmware;
					bool compatibleHW = fw->isHardwareCompatible(targetHW);

					if (!compatibleHW)
					{
						bool hwResult = AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "Incompatible Hardware revision", "The selected firmware is made for a different revision that the connected props \
(props are rev " + Firmware::getHwRevNameforHwRev(targetHW) + ", firmware is rev " + Firmware::getHwRevNameforHwRev(fw->hwRev)
+"\nAre you SURE you want to use this firmware ?", "Yes", "No");

						if (!hwResult)
						{
							FirmwareManager::getInstance()->localFirmware.reset();
						}

					}
				}
			}

			

			updateVisibility();
		}
	}
}
