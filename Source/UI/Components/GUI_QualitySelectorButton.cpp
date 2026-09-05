#include "GUI_QualitySelectorButton.h"

#include "ultra-shared/UI/UI_Helpers.h"
//-----------------------------------------------------------------------------

GUI_QualitySelectorButton::GUI_QualitySelectorButton ( const juce::String& _name, const juce::String& _text )
	: juce::TextButton ( _name )
{
	stateTexts = juce::StringArray::fromTokens ( _text, ",", "" );
	stateTexts.trim ();

	if ( stateTexts.size () )
		setButtonText ( stateTexts[ 0 ] );

	enablementChanged ();
}
//-----------------------------------------------------------------------------

void GUI_QualitySelectorButton::enablementChanged ()
{
	juce::TextButton::enablementChanged ();

	setAlpha ( isEnabled () ? 1.0f : 0.5f );
	setMouseCursor ( isEnabled () ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::ParentCursor );
}
//-----------------------------------------------------------------------------

void GUI_QualitySelectorButton::paintButton ( juce::Graphics& g, bool isHover, bool /*isDown*/ )
{
	const auto	b = getLocalBounds ().toFloat ();
	const auto	col = findColour ( colorId );
	const auto	alphaMul = isHover ? 0.65f : 1.0f;

	// Background
	g.setColour ( col.withMultipliedAlpha ( 0.1f * alphaMul ) );
	g.fillRoundedRectangle ( b, UI::corner ( UI::corners::quality_button, b ) );

	// Text
	if ( stateTexts.isEmpty () )
		return;

	g.setFont ( UI::font ( UI::fonts::quality_selector_button ) );

	g.setColour ( col.withMultipliedAlpha ( alphaMul ) );
	g.drawText ( stateTexts[ currentState ], b, juce::Justification::centred, false );
}
//-----------------------------------------------------------------------------

juce::String GUI_QualitySelectorButton::getTooltip ()
{
	if ( const auto& str = juce::TextButton::getTooltip (); str.isNotEmpty () )
		return strings->get ( str + "_tip" );

	return {};
}
//-----------------------------------------------------------------------------

void GUI_QualitySelectorButton::clicked ( const juce::ModifierKeys& modifiers )
{
	if ( clickingChangesState )
	{
		const auto	size = stateTexts.size ();

		if ( modifiers.isPopupMenu () )
			--currentState;
		else
			++currentState;

		if ( currentState < 0 )
			currentState += size;
		else if ( currentState >= size )
			currentState -= size;

		if ( size )
			setButtonText ( stateTexts[ currentState ] );
	}

	juce::TextButton::clicked ( modifiers );
}
//-----------------------------------------------------------------------------

void GUI_QualitySelectorButton::setMultiState ( const juce::String& _state )
{
	const auto	index = stateTexts.indexOf ( _state, true );

	if ( index >= 0 )
		currentState = index;

	repaint ();
}
//-----------------------------------------------------------------------------

juce::String GUI_QualitySelectorButton::getMultiState () const
{
	if ( stateTexts.isEmpty () )
		return {};

	return stateTexts[ currentState ];
}
//-----------------------------------------------------------------------------

void GUI_QualitySelectorButton::setMultiStateInt ( const int _state )
{
	if ( _state < stateTexts.size () )
		currentState = _state;

	repaint ();
}
//-----------------------------------------------------------------------------
