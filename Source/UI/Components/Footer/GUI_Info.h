#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"

#include "GUI_Thumbnail.h"

struct SidTuneInfoEZ;

//-----------------------------------------------------------------------------

class GUI_Info : public juce::Component
{
public:
	GUI_Info ();

	// this
	void setStrings ( const SidTuneInfoEZ& src );

	GUI_Thumbnail	thumbnail { "thumbnail" };

private:
	GUI_Label		title { "", UI::fonts::footer_title, UI::colors::text };
	GUI_Label		author { "", UI::fonts::footer_author, UI::colors::textMuted };
	GUI_Label		released { "", UI::fonts::footer_released, UI::colors::textMuted };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Info )
};
//-----------------------------------------------------------------------------
