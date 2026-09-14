#include "GUI_Popup.h"

#include "ultra-shared/Helpers/PlatformHelper.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-corners.h"

//-----------------------------------------------------------------------------

namespace
{
	constexpr int	closeSize = 28;
	constexpr int	closeInset = 8;

	void keepFocusOnClick ( juce::Component& c )
	{
		c.setMouseClickGrabsKeyboardFocus ( false );

		for ( auto* child : c.getChildren () )
			keepFocusOnClick ( *child );
	}
}
//-----------------------------------------------------------------------------

GUI_Popup::GUI_Popup ( const juce::String& name, const bool _staysOpen )
	: staysOpen ( _staysOpen )
{
	setName ( name );
	setWantsKeyboardFocus ( ! staysOpen );

	if ( staysOpen )
	{
		closeButton = std::make_unique<GUI_SVG_Button> ( "close", juce::StringArray { "about/close" } );

		closeButton->margin = 8.0f;
		closeButton->bckAlpha[ 0 ] = 0.2f;
		closeButton->bckAlpha[ 1 ] = 0.4f;
		closeButton->bckMargin = 2.0f;
		closeButton->setWantsKeyboardFocus ( false );

		closeButton->onClick = [ this ] {	close ();	};

		addAndMakeVisible ( *closeButton );
	}
}
//-----------------------------------------------------------------------------

void GUI_Popup::resized ()
{
	const auto	b = getPanelBounds ();

	shadowPath.clear ();
	shadowPath.addRoundedRectangle ( b.toFloat (), UI::corner ( UI::corners::popup, b.toFloat () ) );

	if ( closeButton )
	{
		closeButton->setBounds ( b.getRight () - closeInset - closeSize, b.getY () + closeInset, closeSize, closeSize );
		closeButton->toFront ( false );
	}
}
//-----------------------------------------------------------------------------

void GUI_Popup::paint ( juce::Graphics& g )
{
	shadow.render ( g, shadowPath );

	g.setColour ( findColour ( juce::TooltipWindow::backgroundColourId ) );
	g.fillPath ( shadowPath );
}
//-----------------------------------------------------------------------------

void GUI_Popup::open ( const juce::Component& owner )
{
	if ( isOpen () )
		return;

	// Clicks on a staysOpen popup must not activate its window, the main
	// window keeps the focus
	if ( staysOpen )
		keepFocusOnClick ( *this );
	else
		previouslyFocused = juce::Component::getCurrentlyFocusedComponent ();

	addToDesktop ( juce::ComponentPeer::windowIsTemporary );

	if ( auto peer = getPeer (), ownerPeer = owner.getPeer (); peer && ownerPeer )
		setWindowOwner ( peer->getNativeHandle (), ownerPeer->getNativeHandle () );

	if ( ! staysOpen )
		focusOnOpen ();
}
//-----------------------------------------------------------------------------

void GUI_Popup::close ()
{
	if ( ! isOpen () )
		return;

	// Hand the focus back only if the popup still holds it; a click outside
	// has already placed it elsewhere
	const auto	restoreFocus = hasKeyboardFocus ( true );

	if ( auto peer = getPeer () )
		setWindowOwner ( peer->getNativeHandle (), nullptr );

	removeFromDesktop ();

	if ( restoreFocus && previouslyFocused != nullptr && previouslyFocused->isShowing () )
		previouslyFocused->grabKeyboardFocus ();

	previouslyFocused = nullptr;
}
//-----------------------------------------------------------------------------

bool GUI_Popup::keyPressed ( const juce::KeyPress& key )
{
	if ( key.getModifiers ().isAnyModifierKeyDown () )
		return unhandledKey && unhandledKey ( key );

	if ( key.isKeyCode ( juce::KeyPress::escapeKey ) )
	{
		close ();
		return true;
	}

	if ( popupKeyPressed ( key ) )
		return true;

	return unhandledKey && unhandledKey ( key );
}
//-----------------------------------------------------------------------------
