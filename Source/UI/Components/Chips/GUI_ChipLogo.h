#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_ChipLogo final : public juce::Component
{
public:
	GUI_ChipLogo ();

	// juce::Component
	void paint ( juce::Graphics& ) override;

	// this
	void setModel ( const std::string& model );

private:
	float			yPadding = 0.0f;
	juce::String	model = "logos/MOS6581";
	juce::Path		path;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ChipLogo )
};
//-----------------------------------------------------------------------------
