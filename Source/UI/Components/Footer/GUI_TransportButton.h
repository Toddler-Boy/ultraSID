#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_SVG_Button.h"

#include "Helpers/Messages.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

class GUI_TransportButton : public GUI_SVG_Button
{
public:
	enum : int8_t
	{
		REPEAT_OFF,
		REPEAT_ALL,
		REPEAT_ONE,
	};

	GUI_TransportButton ( const juce::String& buttonName, const juce::StringArray& svgNames )
		: GUI_SVG_Button ( buttonName, svgNames )
	{
		bckColId = UI::colors::window;
	}
	//-----------------------------------------------------------------------------

	void clicked ( const juce::ModifierKeys& modifiers ) override
	{
		GUI_SVG_Button::clicked ( modifiers );

		bckAlpha[ 0 ] = bckAlpha[ 1 ] = getStage () ? 0.5f : 0.0f;

		msg::Transport { getName () }.send ();
	}
	//-----------------------------------------------------------------------------

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_TransportButton )
};
//-----------------------------------------------------------------------------
