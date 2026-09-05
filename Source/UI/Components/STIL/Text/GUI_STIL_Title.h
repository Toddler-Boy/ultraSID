#pragma once

#include "ultra-shared/Resources/Icons.h"

#include "GUI_STIL_TextBox.h"

//----------------------------------------------------------------------------------

class GUI_STIL_TitleChild final : public juce::Component
{
public:
	GUI_STIL_TitleChild ( juce::String name, juce::String text )
		: rawText ( text )
	{
		name = name.toLowerCase ();
		setName ( name );

		static std::unordered_map<juce::String, juce::String>	titleIcons =
		{
			{ "title",	"stil/title/song"	},
			{ "artist",	"stil/title/artist"	},
		};

		const juce::SharedResourcePointer<Icons>	icons;

		icon = icons->get ( titleIcons[ name ] );
	}

	void paint ( juce::Graphics& g ) override
	{
		auto		b = getLocalBounds ().toFloat ();
		const auto	color = findParentComponentOfClass<GUI_STIL_TextBox> ()->textColor ();

		// Category icon
		{
			auto	cat = b.removeFromLeft ( b.getHeight () );

			g.setColour ( color );

			const auto&	p = UI::getScaledPath ( icon, UI::padded ( cat, UI::paddings::stil_title_icon ) );
			g.fillPath ( p );
		}

		b.removeFromLeft ( UI::paddingDef ( UI::paddings::stil_title_gap ).top );

		g.setFont ( UI::font ( UI::fonts::stil_title ) );
		g.setColour ( color );
		g.drawText ( rawText, b, juce::Justification::centredLeft, true );
	}

	juce::String	rawText;

private:
	juce::String	icon;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_TitleChild )
};
//----------------------------------------------------------------------------------

class GUI_STIL_Title : public GUI_STIL_TextBox
{
public:
	GUI_STIL_Title ()
		: GUI_STIL_TextBox ( UI::colors::stilBoxTitle, {}, UI::fonts::stil_title )
	{
		setName ( "title" );
	}

	void addEntry ( const juce::String& name, const juce::String& text )
	{
		if ( name == "ARTIST" )
		{
			// Find last artist entry and check if text is the same, if so, move
			// it below the titles instead of adding a duplicate
			for ( auto i = rows.size (); i-- > 0; )
			{
				if ( auto row = rows[ i ]; row->getName () == juce::String ( "artist" ) && row->rawText == text )
				{
					rows.move ( i, rows.size () - 1 );
					return;
				}
			}
		}

		textArea.addAndMakeVisible ( rows.add ( new GUI_STIL_TitleChild ( name, text ) ) );
	}

private:
	// The rows replace the text block; their total height feeds the layout
	// and they stack inside the text area
	int layoutText ( const int /*width*/ ) override
	{
		auto	bottom = 0;
		forEachRow ( [ & ] ( GUI_STIL_TitleChild&, const int y, const int rowHeight ) { bottom = y + rowHeight; } );

		return bottom;
	}

	void textLaidOut () override
	{
		forEachRow ( [ & ] ( GUI_STIL_TitleChild& row, const int y, const int rowHeight ) { row.setBounds ( 0, y, textArea.getWidth (), rowHeight ); } );
	}

	// Runs fn with each row's y offset and height; artists get a trailing gap
	// so they stand apart from the titles above
	template <typename Fn>
	void forEachRow ( Fn&& fn )
	{
		const auto	rowHeight = layoutConstant ( "titleRowHeight", 16 );
		const auto	rowGap = layoutConstant ( "titleRowGap", 6 );

		for ( auto y = 0; auto row : rows )
		{
			fn ( *row, y, rowHeight );
			y += rowHeight + ( row->getName () == juce::String ( "artist" ) ) * rowGap;
		}
	}

	juce::OwnedArray<GUI_STIL_TitleChild>	rows;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Title )
};
//----------------------------------------------------------------------------------
