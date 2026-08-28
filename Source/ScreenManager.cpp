/*
  ==============================================================================

    ScreenManager.cpp
    Created: 4 Sep 2018 12:38:29pm
    Author:  Ben

  ==============================================================================
*/

#include "ScreenManager.h"

#include "PropChooserScreen.h"
#include "PropConnectScreen.h"
#include "FirmwareChooserScreen.h"
#include "UploadScreen.h"
#include "EndScreen.h"

ScreenManager::ScreenManager() :
	curScreen(nullptr),
	prevBT("Previous",.5,Colours::lightgrey)
{
	screens.add(new PropChooserScreen());
	screens.add(new PropConnectScreen());
	screens.add(new FirmwareChooserScreen());
	screens.add(new UploadScreen());
	screens.add(new EndScreen());

	setScreen(screens[0]);

	addChildComponent(prevBT);
	prevBT.addListener(this);

	for (auto &s : screens) s->addScreenListener(this);
}

ScreenManager::~ScreenManager()
{
}

void ScreenManager::paint(Graphics & g)
{
	Rectangle<int> r = getLocalBounds().removeFromTop(headerHeight).reduced(40, 10);
	int numScreens = screens.size();
	Colour bgColor = Colour(0xFF555555);
	const int circleSize = 8;
	int index = screens.indexOf(curScreen);

	for (int i = 0; i < numScreens; i++)
	{
		Colour c = i <= index ? Colours::limegreen : bgColor;
		Point<float> p = r.getRelativePoint(i*1.0f / (numScreens - 1), .5f).toFloat();
		Point<float> prevP = r.getRelativePoint((i-1)*1.0f / (numScreens - 1), .5f).toFloat();
		g.setColour(c);
		if (i > 0) g.drawLine(Line<float>(prevP, p), 1);
	}

	for (int i = 0; i < numScreens; i++)
	{
		Colour c = i == index ? Colours::orange:( i<index?Colours::limegreen : bgColor);
		Point<int> p = r.getRelativePoint(i*1.0f / (numScreens - 1), .5f);
		Rectangle<float> rc = r.withSize(circleSize, circleSize).withCentre(p).toFloat();
		g.setColour(c)
; 
		g.fillEllipse(rc);
		g.drawText(screens[i]->getName(), rc.translated(0, -circleSize * 2).withSizeKeepingCentre(100, 20).toFloat(), Justification::centred);

	}
}

void ScreenManager::resized()
{
	Rectangle<int> r = getLocalBounds();
	r.removeFromTop(headerHeight);

	if (curScreen != nullptr)
	{
		curScreen->setBounds(r);
	}

	prevBT.setBounds(r.removeFromLeft(100).withSizeKeepingCentre(20, 20));
	
}

void ScreenManager::reset()
{
	setScreen(screens[0]);
}

void ScreenManager::nextScreen()
{
	int index = screens.indexOf(curScreen);

	if (index < screens.size() - 1) setScreen(screens[index + 1]);
	else setScreen(screens[0]);
}

void ScreenManager::prevScreen()
{
	int index = screens.indexOf(curScreen);
	if (index > 0) setScreen(screens[index - 1]);
}

void ScreenManager::setScreen(AppScreen * s)
{
	if (s == curScreen) return;

	if (curScreen != nullptr)
	{
		removeChildComponent(curScreen);
	}

	curScreen = s;
	
	if (curScreen == nullptr) curScreen = screens[0];

	curScreen->reset();
	addAndMakeVisible(curScreen);
	prevBT.toFront(false);
	prevBT.setVisible(screens.indexOf(curScreen) > 0 && screens.indexOf(curScreen) < screens.size() - 1);

	repaint();
	resized();
}

void ScreenManager::screenFinish(AppScreen * s)
{
	if (curScreen == s) nextScreen();
}

void ScreenManager::gotoScreen(AppScreen::ScreenID id)
{
	for (auto &ts : screens)
	{
		if (ts->id == id)
		{
			setScreen(ts);
			return;
		}
	}
}

bool ScreenManager::keyPressed(const KeyPress& e)
{
#if JUCE_DEBUG
	if (e.getKeyCode() == e.createFromDescription("n").getKeyCode() && e.getModifiers().isCommandDown())
	{
		nextScreen();
	}
#endif

	return false;
}

void ScreenManager::buttonClicked(Button * b)
{
	if (b == &prevBT)
	{
		prevScreen();
	}
}
