#pragma once

#include "GUI_STIL_TextBox.h"

//----------------------------------------------------------------------------------

class GUI_STIL_Comment final : public GUI_STIL_TextBox
{
public:
	GUI_STIL_Comment ( const juce::String& text )
		: GUI_STIL_TextBox ( UI::colors::stilBoxComment, text, UI::fonts::stil_comment )
	{
		setName ( "comment" );
	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Comment )
};
//----------------------------------------------------------------------------------
