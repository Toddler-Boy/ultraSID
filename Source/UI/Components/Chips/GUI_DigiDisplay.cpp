#include <JuceHeader.h>

#include "GUI_DigiDisplay.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "chip-constants.h"

//-----------------------------------------------------------------------------

// Tweakables for the eye: how far back the lock searches, how far the
// triggered edge sits from the left, what still counts as silence, and how
// many quiet updates the last waveform survives before the display clears
static constexpr auto	searchDepth = GUI_DigiDisplay::blockLength;
static constexpr auto	preTriggerSamples = 64;
static constexpr auto	edgeThreshold = 4;
static constexpr auto	holdFrames = 3;

//-----------------------------------------------------------------------------

GUI_DigiDisplay::GUI_DigiDisplay ()
	: juce::Component ( "digi" )
	, shadow ( juce::Colours::black.withAlpha ( 0.5f ), 5.0f, { 0.0f, 3.0f } )
{
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_DigiDisplay::resized ()
{
	shadowPath.clear ();

	const auto	b = getLocalBounds ().toFloat ();
	shadowPath.addRoundedRectangle ( b, UI::corner ( UI::corners::chip_states, b ) );

	// Repaint background to update intendation
	getParentComponent ()->getChildComponent ( 0 )->repaint ();
}
//-----------------------------------------------------------------------------

void GUI_DigiDisplay::paint ( juce::Graphics& g )
{
	const auto	b = getLocalBounds ().toFloat ();

	auto	gs = GUI_RoundedClip ( g, b, UI::corner ( UI::corners::chip_states, b ) );

	const auto	col = findColour ( colorId );
	g.fillAll ( col.interpolatedWith ( juce::Colours::black, UI::chip::backBlack ) );

	g.setColour ( col );
	g.strokePath ( path, juce::PathStrokeType ( UI::lineWidth ( UI::lines::chip_states ), juce::PathStrokeType::JointStyle::beveled ) );

	shadow.render ( g, shadowPath );
}
//-----------------------------------------------------------------------------

void GUI_DigiDisplay::setColorId ( const int colId )
{
	colorId = colId;
}
//-----------------------------------------------------------------------------

void GUI_DigiDisplay::reset ()
{
	hasLockRef = false;
	lastOffset = -1;
	armed = false;
	pendingAnchor = -1;
	quietFrames = 0;

	path.clear ();
	repaint ();
}
//-----------------------------------------------------------------------------

// Sum of absolute differences with early exit, the lock's match score
static int sad ( const int8_t* a, const int8_t* b, const int count, const int abortAbove )
{
	auto	sum = 0;

	for ( auto i = 0; i < count; ++i )
	{
		sum += std::abs ( int ( a[ i ] ) - int ( b[ i ] ) );

		if ( sum > abortAbove )
			return sum;
	}

	return sum;
}
//-----------------------------------------------------------------------------

void GUI_DigiDisplay::setData ( const int8_t* data, const int lookback )
{
	if ( ! isVisible () || ! data )
		return;

	// Same play position as last time = nothing new to show
	if ( lookback == lastOffset )
		return;

	// A backward or leaping play position is a seek (scrubbing): all lock and
	// trigger state refers to another spot in the tune, start fresh. A stale
	// pendingAnchor would otherwise freeze the display until playback reaches
	// its old absolute position again
	const auto	delta = lookback - lastOffset;
	if ( lastOffset >= 0 && ( delta < 0 || delta > blockLength * 4 ) )
	{
		hasLockRef = false;
		armed = false;
		pendingAnchor = -1;
		quietFrames = 0;
	}

	lastOffset = lookback;

	// A fired trigger waits here until the pre-trigger window is complete
	if ( pendingAnchor >= 0 )
	{
		if ( pendingAnchor > lookback )
			return;

		drawWindow ( data - ( lookback - pendingAnchor ) );
		pendingAnchor = -1;
		return;
	}

	// A quiet block arms the trigger so the next burst anchors near the left
	// edge; the last waveform survives short gaps before the display clears
	const auto	val = data[ 0 ];
	if ( std::ranges::all_of ( data, data + blockLength, [ val ] ( const auto d ) { return d == val; } ) )
	{
		if ( ! path.isEmpty () && ++quietFrames >= holdFrames )
			reset ();

		armed = true;
		quietLevel = val;
		return;
	}

	if ( armed )
	{
		// Fire on the first sample that leaves the quiet level, rising or
		// falling; anything below the threshold still counts as silence
		auto	edge = -1;

		for ( auto i = 0; i < blockLength; ++i )
		{
			if ( std::abs ( int ( data[ i ] ) - int ( quietLevel ) ) >= edgeThreshold )
			{
				edge = i;
				break;
			}
		}

		// Sub-threshold wiggle is still silence, it counts toward the hold
		if ( edge < 0 )
		{
			if ( ! path.isEmpty () && ++quietFrames >= holdFrames )
			{
				reset ();
				armed = true;
			}

			return;
		}

		armed = false;

		const auto	anchor = std::max ( 0, lookback + edge - preTriggerSamples );

		// An edge late in the block has no full window behind it yet; wait
		// a frame rather than drawing it at a random position
		if ( anchor > lookback )
		{
			pendingAnchor = anchor;
			return;
		}

		drawWindow ( data - ( lookback - anchor ) );
		return;
	}

	// Lock onto the previously drawn window: scan backwards sample by sample
	// for the best match. A real lock has a near-zero score, so an older
	// window must beat the newest one hard; a lax bar lets bursty content
	// match its quiet stretch on any old window and re-show stale bursts.
	// The scan stops short of the last feed's window: its perfect score would
	// redraw it verbatim and discard the fresh block
	const auto		avail = std::min ( { searchDepth, lookback, delta - 1 } );
	const int8_t*	window = data;

	if ( hasLockRef )
	{
		// A true lock stays below a few counts of error per sample; above that
		// it is defeat, show the current buffer instead of a bad old window
		constexpr auto	maxLockSad = blockLength * 6;

		const auto	newestSad = sad ( data, lockRef.data (), blockLength, std::numeric_limits<int>::max () );
		const auto	threshold = std::min ( newestSad / 4, maxLockSad );

		auto	bestSad = threshold;

		// Candidates anchor to the newest window, never to an already chosen
		// one, or the offsets would compound right out of the buffer
		for ( auto offset = 1; offset <= avail; ++offset )
		{
			const auto	candidate = data - offset;

			if ( const auto s = sad ( candidate, lockRef.data (), blockLength, bestSad ); s < bestSad )
			{
				bestSad = s;
				window = candidate;
			}
		}
	}

	drawWindow ( window );
}
//-----------------------------------------------------------------------------

void GUI_DigiDisplay::drawWindow ( const int8_t* window )
{
	std::copy_n ( window, blockLength, lockRef.begin () );
	hasLockRef = true;
	quietFrames = 0;

	path.clear ();

	const auto	h2 = getHeight () * 0.5f - 2.0f;
	const auto	zero = ( 1.0f / 128.0f ) * h2;
	const auto	w = getWidth ();

	// One vertex per pixel column at the column's mean sample: the capture is
	// already low-passed, so box decimation loses nothing the eye could see,
	// and the single polyline strokes as thin as the voice displays
	const auto	samplesPerPixel = static_cast<float> ( blockLength ) / static_cast<float> ( w );

	for ( auto x = 0; x < w; ++x )
	{
		const auto	startSample = static_cast<int> ( static_cast<float> ( x ) * samplesPerPixel );
		const auto	endSample = std::clamp ( static_cast<int> ( static_cast<float> ( x + 1 ) * samplesPerPixel ), startSample + 1, blockLength );

		auto	sum = 0;
		for ( auto i = startSample; i < endSample; ++i )
			sum += window[ i ];

		const auto	mean = static_cast<float> ( sum ) / static_cast<float> ( endSample - startSample );
		const auto	y = h2 - mean * zero;

		if ( x == 0 )
			path.startNewSubPath ( static_cast<float> ( x ), y );
		else
			path.lineTo ( static_cast<float> ( x ), y );
	}

	repaint ();
}
//-----------------------------------------------------------------------------
