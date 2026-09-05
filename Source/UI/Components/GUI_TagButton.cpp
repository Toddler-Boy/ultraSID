#include "GUI_TagButton.h"

#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/Resources/Theme.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"

//----------------------------------------------------------------------------------


GUI_TagButton::GUI_TagButton ( const juce::String& name, const int _colorId )
	: GUI_TagButton ( name, _colorId, Roles {} )
{
}
//----------------------------------------------------------------------------------

GUI_TagButton::GUI_TagButton ( const juce::String& name, const int _colorId, const Roles _roles )
	: juce::ToggleButton ( name )
	, colorId ( _colorId )
	, roles ( _roles )
{
	// Clicks don't move the keyboard focus
	setMouseClickGrabsKeyboardFocus ( false );
	enablementChanged ();
}
//----------------------------------------------------------------------------------

void GUI_TagButton::enablementChanged ()
{
	juce::ToggleButton::enablementChanged ();

	setAlpha ( isEnabled () ? 1.0f : 0.5f );
	setMouseCursor ( isEnabled () ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::ParentCursor );
}
//----------------------------------------------------------------------------------

void GUI_TagButton::setButtonWidth ()
{
	const auto&	text = strings->get ( getName () );

	textWidth = juce::GlyphArrangement::getStringWidth ( UI::font ( roles.font ), text );
	textWidthOn = juce::GlyphArrangement::getStringWidth ( UI::fontSized ( UI::fontDef ( roles.font ).size, UI::fontDef ( roles.font ).weight + 100 ), text );

	setSize ( std::ceil ( std::max ( textWidthOn, textWidth ) ) + getHeight () * 2, getHeight ());
}
//----------------------------------------------------------------------------------

void GUI_TagButton::resized ()
{
	juce::ToggleButton::resized ();
	setButtonWidth ();
}
//----------------------------------------------------------------------------------

void GUI_TagButton::paint ( juce::Graphics& g )
{
	const auto	name = getName ();

	auto	b = getLocalBounds ().toFloat ();
	const auto	isOn = getToggleState () && isEnabled ();
	const auto	color = isEnabled () ? findColour ( colorId ) : UI::getShade ( 0.75f );
	const auto	textCol = isOn ? UI::getShade ( 0.0f ) : color;
	const auto	cornerRadius = UI::corner ( roles.corner, b.getHeight () );

	// Background
	if ( isOn )
	{
		g.setColour ( color );
		GUI_LookAndFeel::drawOutlinedRect ( g, b, cornerRadius, UI::lineWidth ( roles.lineOn ), UI::getShade ( 1.0f ).withMultipliedAlpha ( 0.5f ) );
	}

	// Outline
	if ( const auto lineWidth = UI::lineWidth ( roles.line ); ! isOn && UI::lines::visible ( lineWidth ) )
	{
		g.setColour ( color.withMultipliedAlpha ( 0.5f ) );

		if ( isEnabled () )
		{
			GUI_LookAndFeel::drawOutline ( g, b, cornerRadius, lineWidth );
		}
		else
		{
			juce::Path	outline;

			outline.addRoundedRectangle ( b.reduced ( lineWidth / 2.0f ), cornerRadius - lineWidth / 2.0f );

			const auto	dashUnit = lineWidth * 2.2f;
			const auto	gapUnit = lineWidth * 1.6f;

			auto	period = dashUnit + gapUnit;
			auto	perimeter = outline.getLength ();
			auto	count = std::max ( 1, juce::roundToInt ( perimeter / period ) );
			auto	adjusted = perimeter / count;
			auto	dashLen = adjusted * ( dashUnit / period );
			auto	gapLen = adjusted - dashLen;

			const float dashLengths[] = { dashLen, gapLen };

			juce::PathStrokeType	stroke ( lineWidth );

			juce::Path	dashed;
			stroke.createDashedStroke ( dashed, outline, dashLengths, 2 );

			g.fillPath ( dashed );
		}
	}

	// Icon & Text
	g.setColour ( textCol );

	auto	h = b.getHeight ();
	b.reduce ( 0.0f, h / 4.0f );
	h = b.getHeight ();

	const auto	totalWidth = h + 4.0f + ( isOn ? textWidthOn : textWidth );

	b = b.withSizeKeepingCentre ( totalWidth, h ).translated ( -1.0f, 0 );

	const auto	iconRect = b.removeFromLeft ( h );
	const auto&	iconName = stateIcons.size () == 2 ? stateIcons[ isOn ? 1 : 0 ] : name;
	const auto& p = UI::getScaledPath ( icons->get ( iconName ), iconRect );
	g.fillPath ( p );

	// Strike-through if disabled; top-left to bottom-right, so it crosses the
	// rocket icon's diagonal instead of aligning with it
	if ( ! isEnabled () )
		g.drawLine ( { iconRect.getTopLeft (), iconRect.getBottomRight () }, 2.0f );

	b.removeFromLeft ( 4.0f );

	g.setFont ( isOn ? UI::fontSized ( UI::fontDef ( roles.font ).size, UI::fontDef ( roles.font ).weight + 100 ) : UI::font ( roles.font ) );
	g.drawText ( strings->get ( name ), b, juce::Justification::centredLeft, false );
}
//----------------------------------------------------------------------------------

juce::String GUI_TagButton::getTooltip ()
{
	return strings->get ( getName () + "_tip" );
}
//----------------------------------------------------------------------------------
