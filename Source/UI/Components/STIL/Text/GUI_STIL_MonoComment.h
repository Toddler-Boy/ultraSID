#pragma once

#include "GUI_STIL_TextBox.h"

//----------------------------------------------------------------------------------

class GUI_STIL_MonoComment final : public GUI_STIL_TextBox
{
public:
	GUI_STIL_MonoComment ( const juce::String& text )
		: GUI_STIL_TextBox ( UI::colors::stilBoxMono, text, UI::fonts::stil_mono )
	{
		setName ( "mono" );
	}

private:
	int layoutText ( const int width ) override
	{
		const auto	def = UI::fontDef ( UI::fonts::stil_mono );

		juce::AttributedString as;
		as.setWordWrap ( juce::AttributedString::WordWrap::none );
		as.setJustification ( juce::Justification::left );

		as.append ( rawText, UI::monoFont ( def.size, def.weight ), textColor () );

		textArea.setBlock ( as, width );

		return textArea.textHeight ();
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_MonoComment )
};
//----------------------------------------------------------------------------------
