/*
  ==============================================================================

    Screen.cpp
    Created: 4 Sep 2018 12:38:33pm
    Author:  Ben

  ==============================================================================
*/

#include "Screen.h"

AppScreen::AppScreen(const String &screenName, ScreenID id) :
	Component(screenName),
	id(id)
{
}

AppScreen::~AppScreen()
{
}

void AppScreen::paint(Graphics &)
{
	//g.setColour(Colours::yellow);
	//g.drawText(getName(), getLocalBounds().toFloat(), Justification::centred);
}

void AppScreen::resized()
{
}
