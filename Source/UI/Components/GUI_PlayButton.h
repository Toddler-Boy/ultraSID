#pragma once

#include "ultra-shared/UI/Components/GUI_SVG_Button.h"

//-----------------------------------------------------------------------------

class GUI_PlayButton final : public GUI_SVG_Button
{
public:
	GUI_PlayButton ()
		: GUI_SVG_Button ( "play", { "playlist/play" } )
	{
		bckMargin = 6.0f;
		bckColId = UI::colors::window;
		bckAlpha[ 0 ] = 1.0f;
		bckAlpha[ 1 ] = 1.0f;

		getProperties ().set ( "focusRadius", 1000 );
	}
	//-----------------------------------------------------------------------------

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_PlayButton )
};
//-----------------------------------------------------------------------------
