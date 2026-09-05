#include "GUI_QualitySelector.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_QualitySelector::GUI_QualitySelector ()
{
	setName ( "qualitySelector" );

	constexpr auto	totalHeight = ( 80 * 5 ) + ( 24 + 15 + 20 );

	setSize ( 400 + 24, totalHeight + 24 );

	addAndMakeVisible ( qualityLabel );

	for ( auto i = 0; auto& qb : qButs )
	{
		addAndMakeVisible ( qb );

		qb.onClick = [ &qb, i, this ]
		{
			if ( ! qb.getToggleState () )
				return;

			if ( quality == i )
				return;

			quality = i;

			if ( qualityChanged )
				qualityChanged ( i );
		};

		++i;
	}
}
//-----------------------------------------------------------------------------

void GUI_QualitySelector::resized ()
{
	auto	b = getLocalBounds ().reduced ( 12 );

	shadowPath.clear ();
	shadowPath.addRoundedRectangle ( b.toFloat (), UI::corner ( UI::corners::quality_selector, b.toFloat () ) );

	b.reduce ( 10, 10 );

	b.removeFromTop ( 5 );
	qualityLabel.setBounds ( b.removeFromTop ( 24 ).translated ( 10, 0 ) );
	b.removeFromTop ( 10 );

	for ( auto& qb : qButs )
		qb.setBounds ( b.removeFromTop ( 80 ) );
}
//-----------------------------------------------------------------------------

void GUI_QualitySelector::paint ( juce::Graphics& g )
{
	shadow.render ( g, shadowPath );

	const auto	col = findColour ( juce::TooltipWindow::backgroundColourId );
	g.setColour ( col );
	g.fillPath ( shadowPath );
}
//-----------------------------------------------------------------------------

void GUI_QualitySelector::setQuality ( const int _quality )
{
	quality = _quality;

	for ( auto i = 0; auto& qb : qButs )
		qb.setToggleState ( i++ == _quality, juce::dontSendNotification );
}
//-----------------------------------------------------------------------------

void GUI_QualitySelector::open ()
{
	if ( isOpen () )
		return;

	previouslyFocused = juce::Component::getCurrentlyFocusedComponent ();

	addToDesktop ( juce::ComponentPeer::windowIsTemporary );

	if ( juce::isPositiveAndBelow ( quality, int ( std::size ( qButs ) ) ) )
		qButs[ quality ].grabKeyboardFocus ();
}
//-----------------------------------------------------------------------------

void GUI_QualitySelector::close ()
{
	if ( ! isOpen () )
		return;

	// Hand the focus back only if the selector still holds it; a click outside
	// has already placed it elsewhere
	const auto	restoreFocus = hasKeyboardFocus ( true );

	removeFromDesktop ();

	if ( restoreFocus && previouslyFocused != nullptr && previouslyFocused->isShowing () )
		previouslyFocused->grabKeyboardFocus ();

	previouslyFocused = nullptr;
}
//-----------------------------------------------------------------------------

bool GUI_QualitySelector::keyPressed ( const juce::KeyPress& key )
{
	if ( key.getModifiers ().isAnyModifierKeyDown () )
		return unhandledKey && unhandledKey ( key );

	if ( key.isKeyCode ( juce::KeyPress::escapeKey ) )
	{
		close ();
		return true;
	}

	const auto	up = key.isKeyCode ( juce::KeyPress::upKey );
	const auto	down = key.isKeyCode ( juce::KeyPress::downKey );

	if ( ! up && ! down )
		return unhandledKey && unhandledKey ( key );

	// Move the focus from the focused (or selected) quality, wrapping around
	const auto	count = int ( std::size ( qButs ) );

	auto	focused = quality;
	for ( auto i = 0; auto& qb : qButs )
	{
		if ( qb.hasKeyboardFocus ( false ) )
			focused = i;

		++i;
	}

	qButs[ ( focused + ( down ? 1 : count - 1 ) ) % count ].grabKeyboardFocus ();
	return true;
}
//-----------------------------------------------------------------------------

GUI_QualitySelector::QualityButton::QualityButton ( const juce::String& _name, const int _colorId )
	: juce::ToggleButton ( _name )
	, colorId ( _colorId )
{
	setRadioGroupId ( 1, juce::dontSendNotification );

	setClickingTogglesState ( true );
	setToggleable ( true );
}
//-----------------------------------------------------------------------------

void GUI_QualitySelector::QualityButton::paintButton ( juce::Graphics& g, bool isHover, bool /*isDown*/ )
{
	const auto	b = getLocalBounds ().toFloat ();

	// Background (if hovered or keyboard-focused)
	if ( isHover || hasKeyboardFocus ( false ) )
	{
		g.setColour ( UI::getShade ( UI::shades::hover ) );
		g.fillRoundedRectangle ( b, UI::corner ( UI::corners::quality_selector_button, b ) );
	}

	auto	r = b.reduced ( 12.0f, 4.0f );

	const auto	lineH = r.getHeight () / 2.0f;

	// Selection circle
	{
		const auto	col = findColour ( UI::colors::fxReal + colorId );
		g.setColour ( getToggleState () ? col.withMultipliedAlpha ( 0.2f ) : juce::Colours::black.withAlpha ( 0.33f ) );

		const auto	circle = r.removeFromLeft ( lineH );

		g.fillEllipse ( circle.withSizeKeepingCentre ( 20.0f, 20.0f ) );

		if ( getToggleState () )
		{
			g.setColour ( col.withMultipliedAlpha ( 0.5f ) );
			g.fillEllipse ( circle.withSizeKeepingCentre ( 10.0f, 10.0f ) );
		}

		r.removeFromLeft ( lineH / 3 );
	}

	{
		const auto	label = getName ().substring ( 0, 1 ).toUpperCase () + getName ().substring ( 1 );

		g.setColour ( findColour ( UI::colors::text ) );
		g.setFont ( UI::font ( UI::fonts::quality_button ) );
		g.drawText ( label, r.removeFromTop ( 28 ), juce::Justification::bottomLeft, false );
	}

	{
		const auto&	description = strings->get ( "footer/quality/" + getName () );

		g.setColour ( findColour ( UI::colors::textMuted ) );
		g.setFont ( UI::font ( UI::fonts::quality_selector_help ) );
		g.drawFittedText ( description, r.toNearestInt (), juce::Justification::topLeft, 2, 1.0f );
	}
}
//-----------------------------------------------------------------------------
