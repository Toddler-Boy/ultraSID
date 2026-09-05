#pragma once

#include <JuceHeader.h>

#include "GUI_DataHistory.h"
#include "GUI_FilterMode.h"

//-----------------------------------------------------------------------------

class GUI_Filter final : public juce::Component
{
public:
	GUI_Filter ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void setState ( const uint8_t filterMode, const bool used );
	void addCutoff ( const float cutoff );
	void addResonance ( const float cutoff );
	void dataAdded ();

private:
	bool	used = false;

	GUI_FilterMode	mode;
	GUI_DataHistory	cutoff;
	GUI_DataHistory	resonance;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Filter )
};
//-----------------------------------------------------------------------------
