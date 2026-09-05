#include <JuceHeader.h>

#include "GUI_MainMenuButton.h"

#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_MainMenuButton::GUI_MainMenuButton ( const juce::String& name )
	: juce::Button ( name )
	, icon ( "main-menu/" + name.toLowerCase ())
{
	setName ( name );
	setTitle ( name );
	setButtonText ( "main-menu/" + name );
	setToggleable ( true );
	setRadioGroupId ( 1, juce::dontSendNotification );

	// Clicks don't move the keyboard focus
	setMouseClickGrabsKeyboardFocus ( false );

	// Focus ring concentric to the button's corner
	getProperties ().set ( "focusRadius", UI::corners::name ( UI::corners::main_menu_button ) );
}
//-----------------------------------------------------------------------------

void GUI_MainMenuButton::paintButton ( juce::Graphics& g, bool hover, bool /*down*/)
{
	const auto	active = getToggleState ();

	// The badges derive from the main-menu role: 80% size, 200 more weight
	const auto	badgeDef = UI::fontDef ( UI::fonts::main_menu );

	auto	b = getLocalBounds ().toFloat ();

	// Draw background
	if ( active || hover )
	{
		g.setColour ( UI::getShade ( active ? UI::shades::selected : UI::shades::hover ) );
		g.fillRoundedRectangle ( b, UI::corner ( UI::corners::main_menu_button, b ) );
	}

	b.reduce ( 8.0f, 0.0f );

	// Draw work-badge
	if ( workText.isNotEmpty () )
	{
		const auto	badgeBounds = b.removeFromRight ( b.getHeight () ).reduced ( 0.0f, 7.0f );

		g.setColour ( findColour ( UI::colors::statusInfo ).withMultipliedAlpha ( 0.5f ) );
		GUI_LookAndFeel::drawOutlinedRect ( g, badgeBounds, badgeBounds.getHeight () * 0.5f, UI::lineWidth ( UI::lines::main_menu_badge ), findColour ( UI::colors::text ).withMultipliedAlpha ( 0.05f ) );

		g.setColour ( findColour ( UI::colors::text ) );
		g.setFont ( UI::fontSized ( badgeDef.size * 0.8f, badgeDef.weight + 200 ) );
		g.drawText ( workText, badgeBounds, juce::Justification::centred, false );

		b.removeFromRight ( 4.0f );
	}

	// Draw error-badge
	if ( errorText.isNotEmpty () )
	{
		auto	badgeBounds = b.removeFromRight ( b.getHeight () * 1.4f ).reduced ( 0.0f, 7.0f );

		g.setColour ( findColour ( UI::colors::statusError ).withMultipliedAlpha ( 0.5f ) );
		GUI_LookAndFeel::drawOutlinedRect ( g, badgeBounds, badgeBounds.getHeight () * 0.5f, UI::lineWidth ( UI::lines::main_menu_badge ), findColour ( UI::colors::text ).withMultipliedAlpha ( 0.05f ) );

		badgeBounds.reduce ( badgeBounds.getHeight () / 5.0f, 0.0f );

		g.setColour ( findColour ( UI::colors::text ) );
		g.fillPath ( UI::getScaledPath ( icons->get ( "status/error" ), badgeBounds.removeFromLeft ( badgeBounds.getWidth () * 0.45f ), 0, 0.15f ) );

		g.setFont ( UI::fontSized ( badgeDef.size * 0.8f, badgeDef.weight + 200 ) );
		g.drawText ( errorText, badgeBounds, juce::Justification::centredLeft, false );
	}

	g.setColour ( findColour ( active || hover ? UI::colors::text : UI::colors::textMuted ) );

	// Draw icon
	g.fillPath ( UI::getScaledPath ( icons->get ( icon ), b.removeFromLeft ( b.getHeight () * 0.6f ), 0, 0.2f ) );
	b.removeFromLeft ( 2.0f );

	// Draw text
	{
		g.setFont ( UI::font ( UI::fonts::main_menu ) );
		g.drawText ( strings->get ( getButtonText () ), b, juce::Justification::centredLeft, false);
	}
}
//-----------------------------------------------------------------------------

void GUI_MainMenuButton::setBadgeText ( const int count, juce::String& text )
{
	juce::String	newText;

	if ( count > 0 )
		newText = count > 99 ? "99+" : juce::String ( count );

	if ( newText == text )
		return;

	text = newText;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_MainMenuButton::setWorkCount ( const int count )
{
	setBadgeText ( count, workText );
}
//-----------------------------------------------------------------------------

void GUI_MainMenuButton::setErrorCount ( const int count )
{
	setBadgeText ( count, errorText );
}
//-----------------------------------------------------------------------------

void GUI_MainMenuButton::enablementChanged ()
{
	setAlpha ( isEnabled () ? 1.0f : 0.5f );
}
//-----------------------------------------------------------------------------
