/*
  ==============================================================================

    UploadScreen.h
    Created: 4 Sep 2018 12:44:51pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "Screen.h"
#include "PropManager.h"
#include "FirmwareManager.h"

class UploadScreen : public AppScreen,
					 public PropManager::AsyncListener
{
public:
	UploadScreen();
	~UploadScreen();

	String text;
	float progression;
	int numSuccess;
	int numErrored;
	void reset() override;

	void paint(Graphics &g) override;

	void newMessage(const PropManager::PropManagerEvent &e) override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UploadScreen)
};