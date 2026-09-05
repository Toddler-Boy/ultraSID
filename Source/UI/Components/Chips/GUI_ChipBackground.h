#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Helpers/MipMap.h"

//-----------------------------------------------------------------------------

class GUI_ChipBackground final : public juce::Component
{
public:
	GUI_ChipBackground ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void addIndentation ( juce::Component* c );
	void setDigiVisible ( const bool shouldBeVisible );

private:
	std::vector<juce::Component*>	indentationComponents;

	MipMap		background;
	bool		digiVisible = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ChipBackground )
};
//-----------------------------------------------------------------------------
