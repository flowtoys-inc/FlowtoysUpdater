/*
  ==============================================================================

    FirmwareChooserScreen.h
    Created: 4 Sep 2018 12:39:25pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "Screen.h"
#include "FirmwareManager.h"

class FirmwareChooserScreen :
	public AppScreen,
	public Button::Listener,
	public ComboBox::Listener,
	public FirmwareManager::AsyncListener
{
public:
	FirmwareChooserScreen();
	~FirmwareChooserScreen();

	Array<std::shared_ptr<Firmware>> fwList; //shared: keeps entries alive across list refreshes (#9)
	ComboBox fwChooser;

	TextButton selectBT;
	TextButton chooseFileBT;
	Label helpBT;

	void updateVisibility();

	void paint(Graphics &g) override;
	void reset() override;
	void mouseDown(const MouseEvent &e) override;

	void resized() override;

	void newMessage(const FirmwareManager::FirmwareManagerEvent & e) override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FirmwareChooserScreen)

	// Inherited via Listener
	virtual void comboBoxChanged(ComboBox * comboBoxThatHasChanged) override;

	// Inherited via Listener
	virtual void buttonClicked(Button *) override;
};