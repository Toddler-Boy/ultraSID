#include <JuceHeader.h>

#include "GUI_Filter.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

#include "../chip-constants.h"

//-----------------------------------------------------------------------------

GUI_Filter::GUI_Filter ()
	: juce::Component ( "filter" )
{
	cutoff.setName ( "cutoff" );
	resonance.setName ( "resonance" );

	addAndMakeVisible ( mode );
	addAndMakeVisible ( cutoff );
	addAndMakeVisible ( resonance );
}
//-----------------------------------------------------------------------------

void GUI_Filter::paint ( juce::Graphics& g )
{
	const auto	colId = used ? UI::colors::filterOn : UI::colors::voiceOff;
	const auto	col = findColour ( colId, true );

	const auto	b = getLocalBounds ().toFloat ();

	g.setColour ( col.interpolatedWith ( juce::Colours::black, UI::chip::backBlack ) );
	g.fillRoundedRectangle ( b, UI::corner ( UI::corners::chip_states, b ) );
}
//-----------------------------------------------------------------------------

void GUI_Filter::setState ( const uint8_t fltMode, const bool _used )
{
	const auto	newUsed = _used && fltMode;
	if ( newUsed != used )
	{
		used = newUsed;
		repaint ();
	}

	mode.setState ( fltMode, used );
	cutoff.reset ();
	resonance.reset ();

	const auto	colId = used ? UI::colors::filterOn : UI::colors::voiceOff;
	cutoff.setColorId ( colId );
	resonance.setColorId ( colId );
}
//-----------------------------------------------------------------------------

void GUI_Filter::addCutoff ( const float _cutoff )
{
	cutoff.addDatapoint ( _cutoff );
}
//-----------------------------------------------------------------------------

void GUI_Filter::addResonance ( const float _resonance )
{
	resonance.addDatapoint ( _resonance );
}
//-----------------------------------------------------------------------------

void GUI_Filter::dataAdded ()
{
	cutoff.closePath ();
	resonance.closePath ();

	cutoff.repaint ();
	resonance.repaint ();
}
//-----------------------------------------------------------------------------
