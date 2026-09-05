#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Overlay strip for a problem the user has to fix in a data file
class GUI_AlertBanner final : public juce::Component
{
public:
	GUI_AlertBanner ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void setMessage ( const juce::String& msg );

private:
	juce::String	message;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_AlertBanner )
};
//-----------------------------------------------------------------------------
