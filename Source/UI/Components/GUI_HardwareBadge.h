#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"

#include "Audio/HardwareOutput.h"

//-----------------------------------------------------------------------------

// Footer pill shown while the USBSID-Pico output is open: green while a tune plays
// through the boards, grey between tunes, red once the boards lost sync
class GUI_HardwareBadge final : public juce::Component
	, public juce::SettableTooltipClient
	, private juce::Timer
{
public:
	GUI_HardwareBadge ();
	~GUI_HardwareBadge () override;

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// juce::TooltipClient
	juce::String getTooltip () override;

private:
	enum class State { hidden, idle, playing, detached };

	// juce::Timer
	void timerCallback () override;

	[[ nodiscard ]] static State stateOf ( const HardwareOutput::Status& s );

	juce::SharedResourcePointer<Strings>			strings;
	juce::SharedResourcePointer<HardwareOutput>		hardware;

	State	state = State::hidden;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_HardwareBadge )
};
//-----------------------------------------------------------------------------
