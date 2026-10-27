/*
  ==============================================================================

    EndScreen.cpp
    Created: 4 Sep 2018 12:44:55pm
    Author:  Ben

  ==============================================================================
*/

#include "EndScreen.h"
#include "ScreenManager.h"

EndScreen::EndScreen() :
	AppScreen("Complete", COMPLETE),
	resetBT("Start again"),
	reconnectBT("Upload same firmware to more props")
{
	resetBT.addListener(this);
	addAndMakeVisible(&resetBT);

	reconnectBT.addListener(this);
	addAndMakeVisible(&reconnectBT);
}

EndScreen::~EndScreen()
{
}

void EndScreen::paint(Graphics &g)
{
	g.setColour(Colours::lightgrey);
	g.drawFittedText("update successful.\nready to flow ...", getLocalBounds(), Justification::centred, 3);
}

void EndScreen::resized()
{
	Rectangle<int> r = getLocalBounds().removeFromBottom(100).withSizeKeepingCentre(100, 20).expanded(10);
	resetBT.setBounds(r.translated(-100, 0));
	reconnectBT.setBounds(r.translated(100, 0).expanded(30, 0));
}

void EndScreen::buttonClicked(Button *b)
{
	if (b == &resetBT)
	{
		screenListeners.call(&ScreenListener::screenFinish, this);
	}
	else if (b == &reconnectBT)
	{
		screenListeners.call(&ScreenListener::gotoScreen, CONNECT);
	}
}
