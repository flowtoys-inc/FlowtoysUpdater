/*
  ==============================================================================

    PropChooserScreen.cpp
    Created: 4 Sep 2018 12:38:47pm
    Author:  Ben

  ==============================================================================
*/

#include "PropChooserScreen.h"
#include "PropManager.h"

PropChooserScreen::PropChooserScreen() :
	AppScreen("Prop Type", PROP_CHOOSE)
{
	for (int i = 1; i < TYPE_MAX; i++)
	{
		int dataSize = 0;
		const char *imageData = BinaryData::getNamedResource((typeStrings[i] + "_png").getCharPointer(), dataSize);
		Image img = ImageCache::getFromMemory(imageData, dataSize);
		DBG(typeStrings[i] + "_png" << " : " << img.getWidth());

		ImageButton *b = new ImageButton();
		b->setImages(true, false, true,
					 img, .8f, Colours::transparentWhite,
					 img, 1, Colours::transparentWhite,
					 img, 1, Colours::yellow.withAlpha(.1f));
		addAndMakeVisible(b);
		buttons.add(b);
		b->addListener(this);

		Label *l = new Label(typeStrings[i], displayNames[i].toLowerCase());
		l->setColour(Label::textColourId, Colours::lightgrey);
		addAndMakeVisible(l);
		labels.add(l);
		l->setJustificationType(Justification::centred);
	}

	setRepaintsOnMouseActivity(true);
}

PropChooserScreen::~PropChooserScreen()
{
}

void PropChooserScreen::paint(Graphics &g)
{
	g.setColour(Colours::lightgrey);
	g.drawFittedText("before connecting your props, select the prop type below.", getLocalBounds().removeFromTop(100).reduced(20), Justification::centred, 3);

	for (auto &b : buttons)
	{
		g.setColour(b->isMouseOver() ? Colours::lightblue : Colours::darkgrey);
		g.drawRoundedRectangle(b->getBounds().toFloat(), 2, 2);
	}
}

void PropChooserScreen::resized()
{
	Rectangle<int> r = getLocalBounds();
	Point<int> cp = r.getCentre();
	const int gap = 200;

	for (int i = 0; i < buttons.size(); i++)
	{
		Point<int> p = cp + Point<int>((int)((i - (buttons.size() / 2.0f) + .5f) * gap), 0);
		buttons[i]->setCentrePosition(p);
		labels[i]->setBounds(Rectangle<int>(0, 0, 100, 20).withCentre(p.translated(0, buttons[i]->getHeight() / 2 + 20)));
	}
}

void PropChooserScreen::buttonClicked(Button *b)
{
	int index = buttons.indexOf(dynamic_cast<ImageButton *>(b)) + 1; //start with notset
	if (index == -1) return;

	PropManager::getInstance()->setSelectedType((PropType)index);

	screenListeners.call(&ScreenListener::screenFinish, this);
}
