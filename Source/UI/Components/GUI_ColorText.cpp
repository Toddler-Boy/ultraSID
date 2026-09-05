#include <algorithm>

#include "UI/Components/GUI_ColorText.h"

//-----------------------------------------------------------------------------

GUI_ColorText::GUI_ColorText ( const UI::fonts::Role role, const juce::AttributedString::WordWrap wrap )
	: fontRole ( role ), wordWrap ( wrap )
{
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_ColorText::paint ( juce::Graphics& g )
{
	juce::AttributedString	as;
	as.setWordWrap ( wordWrap );

	const auto	font = UI::font ( fontRole );

	for ( const auto& segment : segments )
		if ( segment.text.isNotEmpty () )
			as.append ( segment.text, font, findColour ( segment.colorId ) );

	juce::TextLayout	layout;
	layout.createLayout ( as, float ( getWidth () ) - boxExpandX * 2.0f );

	paintBoxes ( g, layout, font.getHeight () * 0.25f );

	layout.draw ( g, getLocalBounds ().toFloat ().reduced ( boxExpandX, boxExpandY ) );
}
//-----------------------------------------------------------------------------

void GUI_ColorText::setText ( const juce::String& text, const int colorId )
{
	setSegments ( { { text, colorId } } );
}
//-----------------------------------------------------------------------------

void GUI_ColorText::setSegments ( std::vector<Segment> newSegments )
{
	segments = std::move ( newSegments );
	repaint ();
}
//-----------------------------------------------------------------------------

// Boxes match glyph runs by color, so a boxed segment must not share its
// color with an unboxed one. Each line of a wrapped segment becomes its own
// pill, the STIL-link technique
void GUI_ColorText::paintBoxes ( juce::Graphics& g, const juce::TextLayout& layout, const float radius )
{
	std::vector<juce::Colour>	colours;
	for ( const auto& segment : segments )
		if ( segment.boxed )
			if ( const auto col = findColour ( segment.colorId ); std::ranges::find ( colours, col ) == colours.end () )
				colours.push_back ( col );

	for ( const auto& colour : colours )
	{
		juce::RectangleList<float>	rects;

		for ( const auto& line : layout )
			for ( const auto& run : line.runs )
				if ( run->colour == colour )
					for ( const auto lineBounds = line.getLineBounds (); const auto& glyph : run->glyphs )
						rects.add ( { boxExpandX + lineBounds.getX () + glyph.anchor.getX (), boxExpandY + lineBounds.getY (), glyph.width, lineBounds.getHeight () } );

		rects.consolidate ();

		juce::Path	p;
		for ( const auto& r : rects )
			p.addRectangle ( r.expanded ( boxExpandX, boxExpandY ) );

		g.setColour ( colour.withAlpha ( boxAlpha ) );
		g.fillPath ( p.createPathWithRoundedCorners ( radius ) );
	}
}
//-----------------------------------------------------------------------------
