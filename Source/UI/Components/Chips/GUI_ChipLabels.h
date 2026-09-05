#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_ChipLabels final : public juce::Component
{
public:
	GUI_ChipLabels ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void addLabel ( juce::Component* c );

private:
	std::vector<juce::Component*>	labels;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ChipLabels )
};
//-----------------------------------------------------------------------------
