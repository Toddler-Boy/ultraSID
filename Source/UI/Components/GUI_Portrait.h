#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Helpers/MipMap.h"

#include "GUI_PortraitPlaceholder.h"

//-----------------------------------------------------------------------------

class GUI_Portrait final : public juce::Component
{
public:
	GUI_Portrait ();

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;

	// this
	void setBitmap ( const juce::String& bitmap, const bool useGoldenBorder );

private:
	juce::String			bitmap;
	MipMap					mipMap;
	GUI_PortraitPlaceholder	placeholder;
	bool					useGoldenBorder = false;

	juce::ColourGradient	goldGradient;
	juce::ColourGradient	silverGradient;
	juce::ColourGradient	whiteSheenGradient;

	void update ();

	juce::VBlankAttachment	vBlankAttachment { this, [ this ] { update (); } };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Portrait )
};
//-----------------------------------------------------------------------------
