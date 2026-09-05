#include "GUI_BadgeOverlay.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

constexpr auto	lifeMS = 800.0;
constexpr auto	fadeInMS = 150.0;
constexpr auto	fadeOutMS = 400.0;
constexpr auto	glidePX = 24.0f;

GUI_BadgeOverlay::GUI_BadgeOverlay ()
	: juce::Component ( "badgeOverlay" )
{
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

juce::Rectangle<int> GUI_BadgeOverlay::areaFor ( const Badge& badge, float& alpha ) const
{
	const auto	t = juce::Time::getMillisecondCounterHiRes () - badge.startMS;

	alpha = 1.0f;
	if ( t < fadeInMS )					alpha = float ( t / fadeInMS );
	else if ( t > lifeMS - fadeOutMS )	alpha = float ( ( lifeMS - t ) / fadeOutMS );

	alpha = std::clamp ( alpha, 0.0f, 1.0f );

	// Positive glides up, negative down
	const auto	drift = glidePX * float ( t / lifeMS ) * ( badge.positive ? -1.0f : 1.0f );

	return badge.box.withCentre ( badge.anchor.translated ( 0, int ( drift ) ) );
}
//-----------------------------------------------------------------------------

void GUI_BadgeOverlay::paint ( juce::Graphics& g )
{
	const auto	clip = g.getClipBounds ();

	for ( const auto& badge : badges )
	{
		// Drawn from animate()'s snapshot, never the clock, so pixels stay
		// inside the erase rect
		const auto	area = badge.lastArea;

		// Anything outside the dirty region is rejected wholesale
		if ( area.isEmpty () || ! area.intersects ( clip ) )
			continue;

		const auto	r = area.toFloat ();

		g.setColour ( findColour ( badge.positive ? UI::colors::statusOk : UI::colors::statusError ).withMultipliedAlpha ( badge.lastAlpha ) );
		g.fillRoundedRectangle ( r, r.getHeight () / 2.0f );

		g.setColour ( findColour ( UI::colors::window ).withMultipliedAlpha ( badge.lastAlpha ) );
		g.setFont ( UI::font ( UI::fonts::badge ) );
		g.drawText ( badge.text, r, juce::Justification::centred, false );
	}
}
//-----------------------------------------------------------------------------

void GUI_BadgeOverlay::spawn ( const juce::Point<int> pos, const int delta )
{
	// Tiles scrolled out of their viewport anchor outside the window
	if ( delta == 0 || ! getLocalBounds ().contains ( pos ) )
		return;

	const auto	text = ( delta > 0 ? "+" : "" ) + juce::String ( delta );

	const auto	font = UI::font ( UI::fonts::badge );
	const auto	w = int ( std::ceil ( juce::GlyphArrangement::getStringWidth ( font, text ) ) ) + 20;
	const auto	h = int ( std::ceil ( font.getHeight () ) ) + 8;

	badges.push_back ( { text, delta > 0, juce::Time::getMillisecondCounterHiRes (), pos, juce::Rectangle<int> ( w, h ) } );

	toFront ( false );
}
//-----------------------------------------------------------------------------

void GUI_BadgeOverlay::animate ()
{
	if ( badges.empty () )
		return;

	const auto	now = juce::Time::getMillisecondCounterHiRes ();

	for ( auto it = badges.begin (); it != badges.end (); )
	{
		auto&	badge = *it;

		if ( now - badge.startMS >= lifeMS )
		{
			repaint ( badge.lastArea.expanded ( 2 ) );
			it = badges.erase ( it );
			continue;
		}

		auto		alpha = 0.0f;
		const auto	area = areaFor ( badge, alpha );

		repaint ( area.getUnion ( badge.lastArea ).expanded ( 2 ) );
		badge.lastArea = area;
		badge.lastAlpha = alpha;

		++it;
	}
}
//-----------------------------------------------------------------------------
