#include "GUI_STIL_TextArea.h"

#include "ultra-shared/Helpers/Regex.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/UI_Menus.h"

//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::mouseMove ( const juce::MouseEvent& e )
{
	// Find the link that the mouse is over
	const auto	oldLink = link;
	link = getLink ( e.position );

	if ( oldLink == link )
		return;

	setMouseCursor ( link ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor );
	repaintHighlight ();
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::mouseExit ( const juce::MouseEvent& /*e*/ )
{
	if ( ! link )
		return;

	link = nullptr;
	setMouseCursor ( juce::MouseCursor::NormalCursor );
	repaintHighlight ();
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::repaintHighlight ()
{
	// The owning box paints the highlight, so the repaint must reach it
	if ( auto parent = getParentComponent () )
		parent->repaint ();
	else
		repaint ();
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::mouseUp ( const juce::MouseEvent& e )
{
	//
	// This is only used for links in the STIL text
	//
	if ( ! link )
		return;

	if ( e.mouseWasDraggedSinceMouseDown () )
	{
		link = nullptr;
		mouseMove ( e );
		return;
	}

	juce::String	tune = "$HVSC$" + link->link;

	//
	// Context menu
	//
	if ( e.mods.isPopupMenu () )
	{
		auto	m = UI::newPopupMenu ( *this );

		// 0 is the tune's own start song
		const auto	tuneKey = tune + ",0";

		UI::menu_AddToPlaylist ( m, juce::StringArray { tuneKey } );
		m.addSeparator ();
		UI::menu_GoToFolder ( m, tune );
		m.addSeparator ();
		UI::menu_ExportTrack ( m, juce::StringArray { tuneKey } );

		UI::showMenuAtMouse ( m, *this );

		return;
	}

	//
	// Regular left click = navigate to the tune and play it
	//
	if ( e.mods.isLeftButtonDown () && ! e.mods.isAnyModifierKeyDown () )
	{
		// TODO: re-implment this differently
		#if 0
		browser.setSearch ( link->link, true );
		browser.setCurrentPlaylist ( nullptr );
		browser.loadTune ( tune, 0, "STIL", -1 );
		#endif

		return;
	}
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::mouseDrag ( const juce::MouseEvent& e )
{
	if ( ! link )
		return;

	if ( e.getDistanceFromDragStart () < 5 )
		return;

	if ( auto dndc = juce::DragAndDropContainer::findParentDragContainerFor ( this ) )
	{
		if ( dndc->isDragAndDropActive () )
			return;

		// Create bitmap
		const auto	baseFont = linkFont ();
		auto	hvFont = baseFont.withHeight ( baseFont.getHeight () * 0.7f * 2.0f );
		auto	imgFont = baseFont.withHeight ( baseFont.getHeight () * 2.0f );

		const auto	hvStr = juce::String ( u8"⚡" );
		const auto	hvW = juce::GlyphArrangement::getStringWidth ( hvFont, hvStr );
		const auto	lnkW = juce::GlyphArrangement::getStringWidth ( imgFont, link->link );
		const auto	h = std::max ( hvFont.getHeight (), imgFont.getHeight () );

		auto	dragImage = juce::Image ( juce::Image::ARGB, int ( hvW + lnkW ) + 21, int ( h ) + 21, true);

		{
			juce::Graphics g ( dragImage );

			const auto	bgCol = findColour ( UI::colors::stilLink );

			auto	b = dragImage.getBounds ().toFloat ();

			// Box
			constexpr auto	radius = 8.0f;

			g.setColour ( bgCol.withMultipliedBrightness ( 0.3f ) );
			g.fillRoundedRectangle ( b, radius );

			// Outline
			constexpr auto	lineW = 3.0f;

			g.setColour ( bgCol );
			g.drawRoundedRectangle ( b.reduced ( lineW / 2.0f ), radius, lineW );

			// Text
			constexpr auto	paddingX = 5.0f;

			g.setColour ( bgCol.withMultipliedLightness ( 1.25f ) );
			b.reduce ( paddingX, 0.0f );

			g.setFont ( hvFont );
			g.drawText ( hvStr, b.removeFromLeft ( hvW ), juce::Justification::centredLeft, false );

			g.setFont ( imgFont );
			g.drawText ( link->link, b, juce::Justification::centredLeft, false );
		}

		dragImage.multiplyAllAlphas ( 0.75f );

		const auto	relPos = e.position - link->bounds.getBounds ().expanded ( 5.0f ).getPosition ();
		const auto	clipped = -( dragImage.getBounds ().toDouble () / 2.0 ).getConstrainedPoint ( relPos.toDouble () ).roundToInt ();

		// The payload speaks the search-drag dialect, so playlists and the
		// playlist grid accept the link; 0 is the tune's own start song
		juce::Array<juce::var>	tunes;
		tunes.add ( "$HVSC$" + juce::String ( link->link ) + ",0" );

		auto*	desc = new juce::DynamicObject ();
		desc->setProperty ( "source", "search" );
		desc->setProperty ( "tunes", tunes );

		dndc->startDragging ( juce::var ( desc ), this, juce::ScaledImage ( dragImage, 2.0 ), false, &clipped );

		setMouseCursor ( juce::MouseCursor::DraggingHandCursor );

		// The drag image carries the whole link; the hover highlight would
		// only linger behind it, surviving even a dropped-nowhere drag
		link = nullptr;
		repaintHighlight ();
	}
}
//---------------------------------------------------------------------------------

GUI_STIL_TextArea::Link* GUI_STIL_TextArea::getLink ( juce::Point<float> mouse )
{
	for ( auto& lnk : links )
		if ( lnk.bounds.containsPoint ( mouse ) )
			return &lnk;

	return {};
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::setTextBlock ( const juce::String& rawText, const int width, const UI::fonts::Role fontRole_, const juce::Colour color )
{
	const auto	linkCol = findColour ( UI::colors::stilLink );

	fontRole = fontRole_;

	const auto	def = UI::fontDef ( fontRole );
	auto	font = UI::font ( fontRole );
	const auto	roleLinkFont = linkFont ();

	links.clear ();
	link = nullptr;

	juce::AttributedString as;
	as.setJustification ( juce::Justification::horizontallyJustified );

	// Detect links to other tunes
	if ( rawText.contains ( "/MUSICIANS/" ) || rawText.contains ( "/GAMES/" ) || rawText.contains ( "/DEMOS/" ) )
	{
		// Use regex to split string to isolate links to other tunes
		const regex::Pattern	linkRegex ( "/MUSICIANS/.*?\\.sid|/DEMOS/.*?\\.sid|/GAMES/.*?\\.sid" );

		auto	offset = 0;

		for ( const auto& seg : linkRegex.segments ( rawText.toStdString () ) )
		{
			const auto	segStr = juce::String ( seg.text );

			// Text
			if ( ! seg.isMatch )
			{
				as.append ( segStr, font, color );
				offset += segStr.length ();
				continue;
			}

			// Link
			as.append ( u8"⚡", roleLinkFont.withPointHeight ( def.size * 0.7f ), linkCol );

			as.append ( segStr, roleLinkFont, linkCol );
			links.push_back ( { seg.text, {}, { offset, offset + segStr.length () + 1 } } );
			offset += segStr.length () + 1;	// the inserted bolt char counts too
		}
	}
	else
	{
		as.append ( rawText, font, color );
	}

	setInterceptsMouseClicks ( ! links.empty (), false );
	textLayout.createLayout ( as, float ( width ) );

	// Set the link bounds
	if ( ! links.empty () )
	{
		for ( const auto& line : textLayout )
			for ( const auto& run : line.runs )
				if ( run->colour == linkCol )
					for ( auto& lnk : links )
						if ( run->stringRange.intersects ( lnk.range ) )
							for ( const auto lineBounds = line.getLineBounds (); auto& gly : run->glyphs )
								lnk.bounds.add ( lineBounds.getX () + gly.anchor.getX (), lineBounds.getY (), gly.width, lineBounds.getHeight () );

		for ( auto& lnk : links )
			lnk.bounds.consolidate ();
	}
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::setBlock ( const juce::AttributedString& as, const int width )
{
	links.clear ();
	link = nullptr;

	setInterceptsMouseClicks ( false, false );
	textLayout.createLayout ( as, float ( width ) );
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextArea::paint ( juce::Graphics& g )
{
	textLayout.draw ( g, getLocalBounds ().toFloat () );
}
//----------------------------------------------------------------------------------

juce::Path GUI_STIL_TextArea::hoverHighlight () const
{
	juce::Path	p;

	if ( ! link )
		return p;

	// Each line of the link becomes an expanded, rounded pill
	const auto	pad = UI::paddingDef ( UI::paddings::stil_link );

	for ( const auto& r : link->bounds )
		p.addRectangle ( r.expanded ( pad.left, pad.top ) );

	return p.createPathWithRoundedCorners ( UI::corner ( UI::corners::stil_link ) );
}
//----------------------------------------------------------------------------------
