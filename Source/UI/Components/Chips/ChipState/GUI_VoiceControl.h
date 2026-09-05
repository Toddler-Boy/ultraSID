#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_VoiceControl final : public juce::Component
{
public:
	GUI_VoiceControl ();

	// JUCE
	void paint ( juce::Graphics& ) override;

	// this
	void setControl ( uint8_t val );

private:
	uint8_t	value = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_VoiceControl )
};
//-----------------------------------------------------------------------------
