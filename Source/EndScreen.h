/*
  ==============================================================================

    EndScreen.h
    Created: 4 Sep 2018 12:44:55pm
    Author:  Ben

  ==============================================================================
*/

#pragma once
#include "Screen.h"

class EndScreen : public AppScreen,
				  public Button::Listener
{
public:
	EndScreen();
	~EndScreen();

	TextButton resetBT;
	TextButton reconnectBT;

	void paint(Graphics &g) override;
	void resized();

	// Inherited via Listener
	virtual void buttonClicked(Button *) override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EndScreen)
};