#include "CustomLookAndFeel.h"

//-----------------------------------------------------------------------------

CustomLookAndFeel::CustomLookAndFeel ()
{
	//
	// Colour theme: everything derives from the two getShade anchors. The window
	// background sits slightly above the darkest shade, which is reserved for the
	// toolbar and footer strips
	//
	const auto	backgroundColour = getShade ( 0.1f );
	const auto	textColour = getShade ( 1.0f );

	setColour ( juce::ResizableWindow::backgroundColourId, backgroundColour );
	setColour ( juce::ListBox::backgroundColourId, juce::Colours::transparentBlack );
	setColour ( juce::ListBox::textColourId, textColour );
	setColour ( juce::Label::textColourId, textColour );
	setColour ( juce::TextEditor::textColourId, textColour );
	setColour ( juce::ToggleButton::textColourId, textColour );

	// Progress bar fill (table cells and footer): the digi green, toned down halfway to black
	setColour ( juce::ProgressBar::foregroundColourId, juce::Colour ( 0xff66ff99 ).interpolatedWith ( juce::Colours::black, 0.5f ) );
}
//-----------------------------------------------------------------------------

void CustomLookAndFeel::drawScrollbar ( juce::Graphics& g, juce::ScrollBar& scrollbar, const int x, const int y, const int width, const int height,
										const bool isScrollbarVertical, const int thumbStartPosition, const int thumbSize, const bool isMouseOver, bool /*isMouseDown*/ )
{
	// Track pinned to the same fixed palette shade as the progress bars
	g.setColour ( getShade ( 0.04f ) );
	g.fillRect ( x, y, width, height );

	juce::Rectangle<int>	thumbBounds;

	if ( isScrollbarVertical )
		thumbBounds = { x, thumbStartPosition, width, thumbSize };
	else
		thumbBounds = { thumbStartPosition, y, thumbSize, height };

	// Pill-shaped thumb in a palette shade
	const auto	thumb = thumbBounds.toFloat ().reduced ( 3.0f );

	const auto	c = getShade ( 0.35f );
	g.setColour ( isMouseOver ? c.brighter ( 0.25f ) : c );
	g.fillRoundedRectangle ( thumb, std::min ( thumb.getWidth (), thumb.getHeight () ) * 0.5f );
}
//-----------------------------------------------------------------------------

void CustomLookAndFeel::drawTableHeaderBackground ( juce::Graphics& g, juce::TableHeaderComponent& header )
{
	// No fill, just a 1px line along the bottom
	g.setColour ( getShade ( 0.4f ) );
	g.fillRect ( header.getLocalBounds ().removeFromBottom ( 1 ) );
}
//-----------------------------------------------------------------------------

void CustomLookAndFeel::drawTableHeaderColumn ( juce::Graphics& g, juce::TableHeaderComponent&, const juce::String& columnName, int /*columnId*/,
												const int width, const int height, bool /*isMouseOver*/, bool /*isMouseDown*/, int /*columnFlags*/ )
{
	juce::Rectangle<int>	area ( width, height );
	area.reduce ( 4, 0 );

	g.setColour ( getShade ( 0.66f ) );
	g.setFont ( juce::Font ( juce::FontOptions ( float ( height ) * 0.5f ) ) );
	g.drawFittedText ( columnName, area, juce::Justification::centredLeft, 1 );
}
//-----------------------------------------------------------------------------
