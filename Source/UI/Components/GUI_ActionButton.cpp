#include "GUI_ActionButton.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//----------------------------------------------------------------------------------

GUI_ActionButton::GUI_ActionButton ( const juce::String& name, const juce::String& text, const int _colorId, const juce::String& tooltip )
	: juce::TextButton ( name, tooltip )
	, colorId ( _colorId )
{
	setButtonText ( text );
	setWantsKeyboardFocus ( false );
	setMouseClickGrabsKeyboardFocus ( false );
}
//----------------------------------------------------------------------------------

void GUI_ActionButton::enablementChanged ()
{
	juce::TextButton::enablementChanged ();

	setAlpha ( isEnabled () ? 1.0f : 0.5f );
}
//----------------------------------------------------------------------------------

void GUI_ActionButton::paint ( juce::Graphics& g )
{
	const auto	isHover = isMouseOver ();

	const auto	b = getLocalBounds ().toFloat ();
	const auto	color = findColour ( isEnabled () ? colorId : UI::colors::textMuted );

	const auto	bck = color.withMultipliedBrightness ( 0.3f );
	const auto	frg = color;

	// Background
	g.setColour ( isHover ? frg : bck );
	g.fillRoundedRectangle ( b, b.getHeight () / 2.0f );

	g.setFont ( UI::font ( UI::fonts::onboarding_button ) );
	g.setColour ( isHover ? bck : frg );
	g.drawText ( getButtonText (), b, juce::Justification::centred );
}
//----------------------------------------------------------------------------------
