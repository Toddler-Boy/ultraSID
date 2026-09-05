#pragma once

#include "ultra-shared/Config/DataSource.h"

#include "GUI_STIL_TextBox.h"

//----------------------------------------------------------------------------------

class GUI_STIL_Bug final : public GUI_STIL_TextBox
{
public:
	GUI_STIL_Bug ( const juce::String& text )
		: GUI_STIL_TextBox ( UI::colors::stilBoxBug, {}, UI::fonts::stil_bug )
	{
		setName ( "bug" );

		// The last line may carry the author in parentheses
		auto	arr = juce::StringArray::fromLines ( text );
		juce::String	author;

		if ( const auto last = arr.size () - 1; last >= 0 )
		{
			if ( auto& end = arr.getReference ( last ); end.startsWithChar ( '(' ) && end.endsWithChar ( ')' ) )
			{
				author = end.substring ( 1 ).dropLastCharacters ( 1 );
				arr.removeRange ( last, 1 );
			}
		}

		rawText = arr.joinIntoString ( "\n" ).trimEnd ();

		if ( author.isNotEmpty () )
			setAuthor ( author, datasource::loadImage ( "Portraits/Bugs/" + author + ".jpg" ), false );
	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Bug )
};
//----------------------------------------------------------------------------------
