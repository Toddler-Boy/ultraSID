#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Shade on the app's colour axis: 0.0 = background, 1.0 = text
inline juce::Colour getShade ( const float blend )
{
	return juce::Colour ( 0xff070912 ).interpolatedWith ( juce::Colour ( 0xffe4e9f4 ), blend );
}
//-----------------------------------------------------------------------------

// App-wide look-and-feel: two colour anchors (background and text) with the
// shades in between interpolated, plus a place to override JUCE's default
// drawing where it doesn't fit the tool

class CustomLookAndFeel final : public juce::LookAndFeel_V4
{
public:
	CustomLookAndFeel ();

	void drawScrollbar ( juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
						 bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown ) override;

	void drawTableHeaderBackground ( juce::Graphics& g, juce::TableHeaderComponent& header ) override;
	void drawTableHeaderColumn ( juce::Graphics& g, juce::TableHeaderComponent& header, const juce::String& columnName, int columnId,
								 int width, int height, bool isMouseOver, bool isMouseDown, int columnFlags ) override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( CustomLookAndFeel )
};
//-----------------------------------------------------------------------------
