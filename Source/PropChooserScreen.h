/*
  ==============================================================================

    PropChooserScreen.h
    Created: 4 Sep 2018 12:38:47pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "Screen.h"

class PropChooserScreen : public AppScreen,
						  public Button::Listener
{
public:
	PropChooserScreen();
	~PropChooserScreen();

	OwnedArray<ImageButton> buttons;
	OwnedArray<Label> labels;

	void paint(Graphics &g) override;
	void resized() override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PropChooserScreen)

	// Inherited via Listener
	virtual void buttonClicked(Button *) override;
};