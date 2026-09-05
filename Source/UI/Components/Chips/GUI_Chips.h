#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Helpers/MipMap.h"

#include "Audio/sid-constants.h"

#include "GUI_Chip.h"

//-----------------------------------------------------------------------------

class GUI_Chips final : public juce::Component
{
public:
	GUI_Chips ();

	// juce::Component
	void resized () override;

	// this
	void setChipsUsed ( int count );
	[[ nodiscard ]] GUI_ChipState& getChipState ( unsigned int chipNo ) { return chipAt ( chipNo ).getChipState (); }
	void setModel ( unsigned int chipNo, const std::string& model );
	void setDigiVisible ( const bool shouldBeVisible );
	void setDigiData ( unsigned int chipNo, int8_t* data, const int lookback ) { chipAt ( chipNo ).setDigiData ( data, lookback ); }
	void setProfile ( unsigned int chipNo, const std::string& chipProfile, const std::string& chipProfileBitmap, const bool goldenBorder );

private:
	// Grows the list to hold count chips. It only ever grows, never shrinks
	void ensureChips ( size_t count );
	[[ nodiscard ]] GUI_Chip& chipAt ( size_t chipNo );

	int			chipsUsed = 0;

	std::vector<std::unique_ptr<GUI_Chip>>	chips;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Chips )
};
//-----------------------------------------------------------------------------
