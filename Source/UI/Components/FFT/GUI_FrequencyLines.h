#pragma once

#include <JuceHeader.h>

#include "Audio/sid-constants.h"

#include "ChipState/GUI_ChipFrequencyLines.h"

//-----------------------------------------------------------------------------

class GUI_FrequencyLines final : public juce::Component
{
public:
	GUI_FrequencyLines ();

	// juce::Component
	void resized () override;

	void setChipsUsed ( int count );
	void reset ( const bool isNTSC );
	void updateState ( const int chipNo, uint8_t* regs, const int regIndex );

private:
	// Grows the list to hold count chips, like GUI_Chips
	void ensureChips ( size_t count );
	[[ nodiscard ]] GUI_ChipFrequencyLines& chipAt ( size_t chipNo );

	int		chipsUsed = 0;

	std::vector<std::unique_ptr<GUI_ChipFrequencyLines>>	chips;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_FrequencyLines )
};
//-----------------------------------------------------------------------------
