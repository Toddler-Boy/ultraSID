#pragma once

#include <JuceHeader.h>

#include "libSidplayEZ/src/EZ/SidTuneInfoEZ.h"

#include "ultra-shared/Resources/Strings.h"

//-----------------------------------------------------------------------------

class GUI_MemoryOverview final : public juce::Component, public juce::TooltipClient
{
public:
	GUI_MemoryOverview ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// juce::TooltipClient
	juce::String getTooltip () override;

	// this
	void setSidTuneInfo ( const SidTuneInfoEZ& _info );

private:
	SidTuneInfoEZ	info;
	juce::SharedResourcePointer<Strings>	strings;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_MemoryOverview )
};
//-----------------------------------------------------------------------------
