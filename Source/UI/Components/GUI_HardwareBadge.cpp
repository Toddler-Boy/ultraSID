#include "GUI_HardwareBadge.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_HardwareBadge::GUI_HardwareBadge ()
{
	setName ( "hardware" );
	setVisible ( false );
	setInterceptsMouseClicks ( true, false );

	startTimer ( 250 );
}
//-----------------------------------------------------------------------------

GUI_HardwareBadge::~GUI_HardwareBadge ()
{
	stopTimer ();
}
//-----------------------------------------------------------------------------

GUI_HardwareBadge::State GUI_HardwareBadge::stateOf ( const HardwareOutput::Status& s )
{
	if ( ! s.open )
		return State::hidden;

	if ( s.detached )
		return State::detached;

	return s.armed ? State::playing : State::idle;
}
//-----------------------------------------------------------------------------

void GUI_HardwareBadge::timerCallback ()
{
	const auto	newState = stateOf ( hardware->status () );

	if ( newState == state )
		return;

	state = newState;

	setVisible ( state != State::hidden );
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_HardwareBadge::paint ( juce::Graphics& g )
{
	const auto	b = getLocalBounds ().toFloat ();

	const auto	colorId = state == State::playing ? UI::colors::voiceOn
						: state == State::detached ? UI::colors::voiceMuted
						: UI::colors::voiceOff;

	const auto	col = findColour ( colorId );

	g.setColour ( col.withMultipliedAlpha ( 0.15f ) );
	g.fillRoundedRectangle ( b, UI::corner ( UI::corners::quality_button, b ) );

	g.setFont ( UI::font ( UI::fonts::quality_selector_button ) );
	g.setColour ( col );
	g.drawText ( "USBSID", b, juce::Justification::centred, false );
}
//-----------------------------------------------------------------------------

juce::String GUI_HardwareBadge::getTooltip ()
{
	const auto	s = hardware->status ();

	switch ( stateOf ( s ) )
	{
		case State::playing:
			return strings->get ( "footer/hardware/playing" ).replace ( "{}", juce::String ( s.boards ) );

		case State::detached:
			return strings->get ( "footer/hardware/detached" );

		default:
			return strings->get ( "footer/hardware/idle" ).replace ( "{}", juce::String ( s.boards ) );
	}
}
//-----------------------------------------------------------------------------
