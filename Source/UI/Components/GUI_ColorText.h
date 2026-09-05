#pragma once

#include <JuceHeader.h>

#include <vector>

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

// Word-wrapping text built from colored segments, for lines that mix meaning
// and color (token help, colorized previews). Plain text is one segment.
// Boxed segments get a soft rounded backdrop in their own color
class GUI_ColorText final : public juce::Component
{
public:
	struct Segment
	{
		juce::String	text;
		int				colorId = 0;	// a UI::colors role
		bool			boxed = false;
	};

	// byChar suits unbroken strings like paths, which byWord would run past
	// the right edge in one invisible line
	explicit GUI_ColorText ( UI::fonts::Role role = UI::fonts::settings_help, juce::AttributedString::WordWrap wrap = juce::AttributedString::WordWrap::byWord );

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void setText ( const juce::String& text, int colorId );
	void setSegments ( std::vector<Segment> newSegments );

private:
	// The boxes outgrow the glyphs; text is inset by the expansion so they
	// stay inside the component instead of clipping at the edges
	static constexpr float	boxAlpha = 0.15f;
	static constexpr float	boxExpandX = 2.5f;
	static constexpr float	boxExpandY = 1.5f;

	void paintBoxes ( juce::Graphics& g, const juce::TextLayout& layout, float radius );

	UI::fonts::Role						fontRole;
	juce::AttributedString::WordWrap	wordWrap;
	std::vector<Segment>				segments;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ColorText )
};
//-----------------------------------------------------------------------------
