#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_ChipProfileIndicator final : public juce::Component
{
public:
	GUI_ChipProfileIndicator ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void setDotColor ( const bool hasProfile );

private:
	juce::Colour	dotCol;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ChipProfileIndicator )
};
//-----------------------------------------------------------------------------
