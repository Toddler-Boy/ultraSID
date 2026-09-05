#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_SVG_Button.h"

//-----------------------------------------------------------------------------

// Three-dot page-menu button, one look across all pages; the tooltip key
// derives from the page name ("<page>/menu")
class GUI_MenuButton final : public GUI_SVG_Button
{
public:
	GUI_MenuButton ( const juce::String& page )
		: GUI_SVG_Button ( "menu", { "menu_button" } )
	{
		tooltips = { page + "/menu" };
		margin = 6.0f;
		bckAlpha[ 0 ] = 0.2f;
		bckAlpha[ 1 ] = 0.4f;
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_MenuButton )
};
//-----------------------------------------------------------------------------
