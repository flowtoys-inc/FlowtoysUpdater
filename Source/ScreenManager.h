/*
  ==============================================================================

    ScreenManager.h
    Created: 4 Sep 2018 12:38:29pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "Screen.h"

class ScreenManager : public Component,
					  public AppScreen::ScreenListener,
					  public Button::Listener
{
public:
	ScreenManager();
	~ScreenManager();

	const int headerHeight = 100;

	void paint(Graphics &g) override;
	void resized() override;

	ArrowButton prevBT;

	OwnedArray<AppScreen> screens;
	AppScreen *curScreen;

	void reset();
	void nextScreen();
	void prevScreen();

	void setScreen(AppScreen *s);

	void screenFinish(AppScreen *s) override;
	void gotoScreen(AppScreen::ScreenID id) override;

	bool keyPressed(const KeyPress &e) override;

	// Inherited via Listener
	virtual void buttonClicked(Button *) override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenManager)
};
