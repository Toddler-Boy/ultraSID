#pragma once

#include <JuceHeader.h>

#include "UI/Components/GUI_Portrait.h"

#include "GUI_ChipBackground.h"
#include "GUI_ChipLogo.h"
#include "GUI_ChipProfileIndicator.h"
#include "GUI_ChipState.h"
#include "GUI_DigiDisplay.h"

//-----------------------------------------------------------------------------

class GUI_Chip final : public juce::Component, public juce::TooltipClient
{
public:
	GUI_Chip ();

	// juce::Component
	void resized () override;

	// juce::TooltipClient
	juce::String getTooltip () override;

	// this
	[[ nodiscard ]] GUI_ChipState& getChipState () { return chipState; }
	void setModel ( const std::string& model );
	void setDigiVisible ( const bool shouldBeVisible );
	void setDigiData ( int8_t* data, const int lookback ) { digiDisplay.setData ( data, lookback ); }
	void setProfile ( const std::string& chipProfile, const std::string& chipProfileBitmap, const bool goldenBorder );

private:
	gin::LayoutSupport	layout { *this };

	GUI_ChipBackground			background;
	GUI_Portrait				portrait;
	GUI_ChipLogo				logo;
	GUI_ChipProfileIndicator	profile;
	GUI_ChipState				chipState;
	GUI_DigiDisplay				digiDisplay;

	bool			isApproved = false;
	juce::String	profileName;
	bool			is6581 = true;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Chip )
};
//-----------------------------------------------------------------------------
