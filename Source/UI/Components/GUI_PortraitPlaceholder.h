#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Icons.h"

//-----------------------------------------------------------------------------

// The stand-in for a missing author picture, tinted with the owner's content
// color
class GUI_PortraitPlaceholder final
{
public:
	void draw ( juce::Graphics& g, const juce::Rectangle<float>& b, const juce::Colour& tint );

private:
	juce::SharedResourcePointer<Icons>	icons;
};
//-----------------------------------------------------------------------------
