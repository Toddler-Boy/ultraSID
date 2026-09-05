#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Helpers/MipMap.h"

//-----------------------------------------------------------------------------

class GUI_Thumbnail : public juce::Button
{
public:
	GUI_Thumbnail ( const juce::String& name );

	// juce::Button
	void paintButton ( juce::Graphics& g, bool isMouseOverButton, bool isButtonDown ) override;

	// this
	void setMipMap ( MipMap& newImage );

private:
	MipMap	mipMap[ 2 ];

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Thumbnail )
};
//-----------------------------------------------------------------------------
