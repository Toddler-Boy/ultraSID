#include "GUI_UndoToast.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

// Message to button gap
constexpr auto	textGap = 20;

// Hover rewind, a multiple of the forward speed
constexpr auto	rewindFactor = 8.0;

static juce::Font undoFont ()
{
	const auto	def = UI::fontDef ( UI::fonts::toast );

	return UI::fontSized ( def.size, def.weight + 200 );
}
//-----------------------------------------------------------------------------

GUI_UndoToast::GUI_UndoToast ()
	: juce::Component ( "undoToast" )
{
	setVisible ( false );
	setAlwaysOnTop ( true );
	setOpaque ( false );
	setMouseClickGrabsKeyboardFocus ( false );
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::paint ( juce::Graphics& g )
{
	// The desktop window clips at its edge, the margin saves the stroke
	const auto	b = getLocalBounds ().toFloat ().reduced ( 1.0f );
	const auto	radius = UI::corner ( UI::corners::toast, b );
	const auto	pad = UI::paddingDef ( UI::paddings::toast_padding );

	const auto	now = juce::Time::getMillisecondCounterHiRes ();

	if ( counting )
	{
		progress = float ( std::clamp ( ( now - startMS ) / ( targetMS - startMS ), 0.0, 1.0 ) );
	}
	else if ( rewinding )
	{
		progress = rewindFrom - float ( ( now - rewindStartMS ) * rewindFactor / durationMS );

		if ( progress <= 0.0f )
		{
			progress = 0.0f;
			rewinding = false;
		}
	}

	g.setColour ( UI::getShade ( 0.1f ) );
	g.fillRoundedRectangle ( b, radius );

	// Countdown bar along the bottom, full = the op commits
	if ( const auto barH = UI::paddingDef ( UI::paddings::toast_progress ).top; barH > 0.0f && progress > 0.0f )
	{
		const GUI_RoundedClip	clip ( g, b, radius );

		g.setColour ( findColour ( UI::colors::accent ) );
		g.fillRect ( juce::Rectangle<float> ( b.getX (), b.getBottom () - barH, b.getWidth () * progress, barH ) );
	}

	g.setColour ( findColour ( UI::colors::text ).withMultipliedAlpha ( 0.1f ) );
	GUI_LookAndFeel::drawOutline ( g, b, radius, UI::lineWidth ( UI::lines::toast ) );

	// Undo pill
	{
		const auto	pill = undoArea.toFloat ();
		auto		accent = findColour ( UI::colors::accent );

		if ( overUndo )
			accent = accent.brighter ( 0.2f );

		g.setColour ( accent );
		g.fillRoundedRectangle ( pill, pill.getHeight () / 2.0f );

		g.setColour ( findColour ( UI::colors::window ) );
		g.setFont ( undoFont () );
		g.drawText ( strings->get ( "toast/undo" ), pill, juce::Justification::centred, false );
	}

	const auto	r = b.withTrimmedLeft ( pad.left ).withTrimmedRight ( pad.right + float ( undoArea.getWidth () ) + textGap );

	g.setColour ( findColour ( UI::colors::text ) );
	g.setFont ( UI::font ( UI::fonts::toast ) );
	g.drawText ( message, r, juce::Justification::centredLeft, true );

	if ( counting )
	{
		if ( now >= targetMS )
		{
			counting = false;

			// Committing tears the toast down, never from inside paint
			juce::MessageManager::callAsync ( [ cb = onExpired ]	{	if ( cb ) cb ();	} );
			return;
		}
	}

	// Chain to the next vblank
	if ( counting || rewinding )
		repaint ();
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::visibilityChanged ()
{
	if ( ! isVisible () )
	{
		counting = false;
		rewinding = false;
	}
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::mouseEnter ( const juce::MouseEvent& /*evt*/ )
{
	// Hover holds the countdown, the bar rewinds out
	if ( counting && progress > 0.0f )
	{
		rewindStartMS = juce::Time::getMillisecondCounterHiRes ();
		rewindFrom = progress;
		rewinding = true;
	}

	counting = false;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::mouseExit ( const juce::MouseEvent& /*evt*/ )
{
	if ( overUndo )
	{
		overUndo = false;
		repaint ();
	}

	// Leaving grants the full window again
	if ( isVisible () && durationMS > 0 )
		startCountdown ( durationMS );
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::mouseMove ( const juce::MouseEvent& evt )
{
	const auto	over = undoArea.contains ( evt.getPosition () );

	if ( over != overUndo )
	{
		overUndo = over;
		repaint ();
	}

	setMouseCursor ( over ? juce::MouseCursor::PointingHandCursor
						  : juce::MouseCursor::NormalCursor );
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::mouseUp ( const juce::MouseEvent& evt )
{
	if ( undoArea.contains ( evt.getPosition () ) && onUndo )
		onUndo ();
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::startCountdown ( const int timeoutMS )
{
	durationMS = timeoutMS;

	startMS = juce::Time::getMillisecondCounterHiRes ();
	targetMS = startMS + timeoutMS;

	progress = 0.0f;
	counting = true;
	rewinding = false;

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_UndoToast::setMessage ( const juce::String& msg )
{
	message = msg;
	progress = 0.0f;

	const auto	pad = UI::paddingDef ( UI::paddings::toast_padding );
	const auto	btnPad = UI::paddingDef ( UI::paddings::toast_button_padding );

	const auto	msgWidth = int ( std::ceil ( juce::GlyphArrangement::getStringWidth ( UI::font ( UI::fonts::toast ), message ) ) );
	const auto	undoWidth = int ( std::ceil ( juce::GlyphArrangement::getStringWidth ( undoFont (), strings->get ( "toast/undo" ) ) ) );

	// The pill is the tallest content, font and paddings define the height
	const auto	buttonWidth = int ( btnPad.left + btnPad.right ) + undoWidth;
	const auto	buttonHeight = int ( std::ceil ( undoFont ().getHeight () ) + btnPad.top + btnPad.bottom );

	setSize ( 2 + int ( pad.left ) + msgWidth + textGap + buttonWidth + int ( pad.right ),
			  2 + buttonHeight + int ( pad.top + pad.bottom ) );

	undoArea = juce::Rectangle<int> ( getWidth () - 1 - int ( pad.right ) - buttonWidth, 1 + int ( pad.top ),
									  buttonWidth, buttonHeight );

	if ( ! isOnDesktop () )
		addToDesktop ( juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresKeyPresses );

	repaint ();
}
//-----------------------------------------------------------------------------
