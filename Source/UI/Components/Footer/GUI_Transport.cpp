#include <JuceHeader.h>

#include "GUI_Transport.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "Database/TuneInfo.h"

//-----------------------------------------------------------------------------

GUI_Transport::GUI_Transport ()
{
	setName ( "transport" );

	addMouseListener ( this, true );

	// Play/Pause
	play.tooltips = { "footer/pause", "footer/play" };

	// Next
	next.margin = 8.0f;
	next.tooltips = { "footer/next" };

	// Previous
	previous.margin = 8.0f;
	previous.tooltips = { "footer/previous" };

	// Shuffle
	shuffle.margin = 8.0f;
	shuffle.tooltips = { "footer/enable_shuffle", "footer/disable_shuffle" };

	// Repeat
	repeat.margin = 8.0f;
	repeat.translation.y = 1.0f;
	repeat.tooltips = { "footer/enable_repeat_all", "footer/enable_repeat_one", "footer/disable_repeat" };

	addAndMakeVisible ( shuffle );
	addAndMakeVisible ( previous );
	addAndMakeVisible ( play );
	addAndMakeVisible ( next );
	addAndMakeVisible ( repeat );

	UI::setFontRole ( time, UI::fonts::transport_text );
	time.setJustificationType ( juce::Justification::centredRight );
	addAndMakeVisible ( time );

	UI::setFontRole ( length, UI::fonts::transport_text );
	length.setJustificationType ( juce::Justification::centredLeft );
	addAndMakeVisible ( length );

	progress.setName ( "progress" );
	progress.setFont ( UI::font ( UI::fonts::transport_text ) );
	progress.setRange ( 0.0, 1.0 );

	addAndMakeVisible ( progress );
}
//-----------------------------------------------------------------------------

GUI_Transport::~GUI_Transport ()
{
	removeMouseListener ( this );
}
//-----------------------------------------------------------------------------

void GUI_Transport::mouseDown ( const juce::MouseEvent& event )
{
	if ( event.eventComponent == &length )
	{
		preferences->set ( "player/show-length", ! preferences->get<bool> ( "player/show-length" ) );
		return;
	}

	mouseDrag ( event );
}
//-----------------------------------------------------------------------------

void GUI_Transport::mouseDrag ( const juce::MouseEvent& event )
{
	if ( event.eventComponent != &progress )
		return;

	const auto	newPosMS = std::min ( int ( progress.getValue () * lengthMS ), renderTimeMS );

	seek ( newPosMS );
}
//-----------------------------------------------------------------------------

void GUI_Transport::setTime ( int _timeMS, int _renderTimeMS )
{
	if ( renderTimeMS != _renderTimeMS )
	{
		renderTimeMS = _renderTimeMS;

		progress.getProperties ().set ( "progress", float ( renderTimeMS ) / float ( lengthMS ) );

		if ( timeMS == _timeMS || progress.isMouseButtonDown () )
			progress.repaint ();
	}

	progress.setLength ( lengthMS );

	if ( timeMS == _timeMS )
		return;

	timeMS = _timeMS;

	auto	str = SID::convertTimeToString ( timeMS );
	time.setText ( str, juce::dontSendNotification );

	if ( lengthMS == INT32_MAX )
	{
		str = "-";
	}
	else
	{
		if ( preferences->get<bool> ( "player/show-length" ) )
			str = SID::convertTimeToString ( lengthMS );
		else
			if ( timeMS < lengthMS )
				str = "-" + SID::convertTimeToString ( lengthMS - timeMS );
			else
				str = "-";
	}

	length.setText ( str, juce::dontSendNotification );

	if ( ! progress.isMouseButtonDown () )
		progress.setValue ( float ( timeMS ) / float ( lengthMS ) );
}
//-----------------------------------------------------------------------------

void GUI_Transport::setLength ( int _timeMS )
{
	if ( ! _timeMS )
		_timeMS = INT32_MAX;

	lengthMS = _timeMS;
}
//-----------------------------------------------------------------------------

void GUI_Transport::seekRelative ( double seconds )
{
	const auto	newPosMS = std::clamp ( timeMS + int ( seconds * 1000.0 ), 0, renderTimeMS );

	seek ( newPosMS );
}
//-----------------------------------------------------------------------------
