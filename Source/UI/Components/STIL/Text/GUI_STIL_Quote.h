#pragma once

#include "ultra-shared/Config/DataSource.h"

#include "GUI_STIL_TextBox.h"

//----------------------------------------------------------------------------------

class GUI_STIL_Quote final : public GUI_STIL_TextBox
{
public:
	GUI_STIL_Quote ( const juce::String& text, const juce::String& speaker = "" )
		: GUI_STIL_TextBox ( UI::colors::stilBoxQuote, text, UI::fonts::stil_quote )
	{
		setName ( "quote" );

		if ( speaker.isNotEmpty () )
			setAuthor ( speaker, datasource::loadImage ( "Portraits/Musicians/" + speaker + ".jpg" ), true );
	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Quote )
};
//----------------------------------------------------------------------------------
