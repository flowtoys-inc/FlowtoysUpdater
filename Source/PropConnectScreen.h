/*
  ==============================================================================

    PropConnectScreen.h
    Created: 4 Sep 2018 12:44:25pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "Screen.h"
#include "PropManager.h"

class PropConnectScreen : 
	public AppScreen,
	public PropManager::AsyncListener,
	public Button::Listener
{
public:
	PropConnectScreen();
	~PropConnectScreen();

	TextButton flashBT;
	Label helpBT;
	Label resetBTLink;

	bool multipleRevisionsDetected;

	void paint(Graphics &g) override;
	void resized() override;
	void reset() override;

	void checkProps();

	void mouseDown(const MouseEvent &e) override;

	void newMessage(const PropManager::PropManagerEvent &e) override;
	

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PropConnectScreen)

		// Inherited via Listener
		virtual void buttonClicked(Button *) override;
};