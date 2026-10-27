/*
  ==============================================================================

    Screen.h
    Created: 4 Sep 2018 12:38:33pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

class AppScreen : public Component
{
public:
	enum ScreenID
	{
		PROP_CHOOSE,
		FW_CHOOSE,
		CONNECT,
		UPLOAD,
		COMPLETE
	};

	AppScreen(const String &screenName, ScreenID id);
	~AppScreen();


	ScreenID id;

	virtual void paint(Graphics &g) override;
	virtual void resized() override;

	virtual void reset() {}

	class ScreenListener
	{
	public:
		virtual ~ScreenListener() {}
		virtual void screenFinish(AppScreen *) {}
		virtual void gotoScreen(ScreenID) {}
	};

	ListenerList<ScreenListener> screenListeners;
	void addScreenListener(ScreenListener *newListener) { screenListeners.add(newListener); }
	void removeScreenListener(ScreenListener *listener) { screenListeners.remove(listener); }

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppScreen)
};