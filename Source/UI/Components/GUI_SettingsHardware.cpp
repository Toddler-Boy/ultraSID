#include "GUI_SettingsHardware.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "Helpers/Messages.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

namespace
{
	constexpr int	listRows = 4;
	constexpr int	disclosureCount = 6;

	juce::String boardsAndSids ( const HardwareOutput::Status& s )
	{
		return juce::String ( s.boards ) + " / " + juce::String ( s.sids );
	}
}
//-----------------------------------------------------------------------------

GUI_SettingsHardware::GUI_SettingsHardware ()
	: juce::Component ( "hardware" )
	, label ( "settings/hardware/boards", UI::fonts::settings_entry, UI::colors::text )
	, help ( "settings/hardware/boards-help", UI::fonts::settings_help, UI::colors::textMuted )
{
	help.setName ( "help" );

	addAndMakeVisible ( label );
	addAndMakeVisible ( help );

	list.setRowHeight ( rowHeight );
	addAndMakeVisible ( list );

	addAndMakeVisible ( refreshButton );
	addAndMakeVisible ( upButton );
	addAndMakeVisible ( downButton );

	refreshButton.onClick = [ this ]	{	refresh ();			};
	upButton.onClick = [ this ]			{	moveSelected ( -1 );	};
	downButton.onClick = [ this ]		{	moveSelected ( 1 );	};

	refresh ();
}
//-----------------------------------------------------------------------------

GUI_SettingsHardware::~GUI_SettingsHardware ()
{
	stopTimer ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::resized ()
{
	auto	b = getLocalBounds ();

	label.setBounds ( b.removeFromTop ( 21 ) );
	help.setBounds ( b.removeFromTop ( 21 ) );
	b.removeFromTop ( 8 );

	list.setBounds ( b.removeFromTop ( listRows * rowHeight ) );
	b.removeFromTop ( 10 );

	auto	buttons = b.removeFromTop ( 28 );
	refreshButton.setBounds ( buttons.removeFromLeft ( 110 ) );
	buttons.removeFromLeft ( 8 );
	upButton.setBounds ( buttons.removeFromLeft ( 110 ) );
	buttons.removeFromLeft ( 8 );
	downButton.setBounds ( buttons.removeFromLeft ( 110 ) );

	b.removeFromTop ( 14 );
	statusBounds = b.removeFromTop ( 21 );

	b.removeFromTop ( 14 );
	disclosureBounds = b;
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::paint ( juce::Graphics& g )
{
	g.setFont ( UI::font ( UI::fonts::settings_field ) );
	g.setColour ( findColour ( UI::colors::text ) );
	g.drawText ( shownStatus, statusBounds.toFloat (), juce::Justification::centredLeft, true );

	// What the mode switches off, one wrapped paragraph per point
	juce::AttributedString	text;
	const auto	font = UI::font ( UI::fonts::settings_help );

	text.append ( strings->get ( "settings/hardware/disclosure-title" ) + "\n", UI::font ( UI::fonts::settings_entry ), findColour ( UI::colors::text ) );

	for ( auto i = 1; i <= disclosureCount; ++i )
		text.append ( juce::String::fromUTF8 ( "\xe2\x80\xa2 " ) + strings->get ( "settings/hardware/disclosure-" + juce::String ( i ) ) + "\n", font, findColour ( UI::colors::textMuted ) );

	text.setWordWrap ( juce::AttributedString::WordWrap::byWord );
	text.draw ( g, disclosureBounds.toFloat () );
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::visibilityChanged ()
{
	if ( isShowing () )
	{
		timerCallback ();
		startTimer ( 500 );
	}
	else
	{
		stopTimer ();
	}
}
//-----------------------------------------------------------------------------

int GUI_SettingsHardware::getNumRows ()
{
	return int ( rows.size () );
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::paintListBoxItem ( const int row, juce::Graphics& g, const int width, const int height, const bool rowIsSelected )
{
	if ( row < 0 || row >= int ( rows.size () ) )
		return;

	const auto&	r = rows[ size_t ( row ) ];

	if ( rowIsSelected )
	{
		g.setColour ( findColour ( UI::colors::text ).withAlpha ( 0.08f ) );
		g.fillRect ( 0, 0, width, height );
	}

	// Check box
	const auto	box = juce::Rectangle<float> ( 8.0f, ( height - 16.0f ) / 2.0f, 16.0f, 16.0f );

	g.setColour ( findColour ( r.selected ? UI::colors::accent : UI::colors::textMuted ) );
	g.drawRoundedRectangle ( box, 3.0f, 1.5f );

	if ( r.selected )
		g.fillRoundedRectangle ( box.reduced ( 4.0f ), 1.5f );

	g.setFont ( UI::font ( UI::fonts::settings_field ) );
	g.setColour ( findColour ( r.attached ? UI::colors::text : UI::colors::textMuted ) );

	auto	text = juce::String ( row + 1 ) + "   " + r.serial;
	if ( ! r.attached )
		text << "   (" << strings->get ( "settings/hardware/not-connected" ) << ")";

	g.drawText ( text, checkWidth, 0, width - checkWidth, height, juce::Justification::centredLeft, true );
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::listBoxItemClicked ( const int row, const juce::MouseEvent& e )
{
	if ( row < 0 || row >= int ( rows.size () ) || e.x >= checkWidth )
		return;

	rows[ size_t ( row ) ].selected = ! rows[ size_t ( row ) ].selected;

	list.repaintRow ( row );
	store ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::timerCallback ()
{
	const auto	text = statusText ();

	if ( text == shownStatus )
		return;

	shownStatus = text;
	repaint ( statusBounds );
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::refresh ()
{
	// Chosen boards keep their stored order, attached or not, then the rest in USB order
	const auto	stored = juce::StringArray::fromTokens ( preferences->get<juce::String> ( "hardware/boards" ), ",", "" );

	juce::StringArray	attached;

	for ( const auto& dev : HardwareOutput::enumerate () )
		if ( ! dev.serial.empty () )
			attached.add ( dev.serial );

	rows.clear ();

	for ( const auto& serial : stored )
		if ( serial.isNotEmpty () )
			rows.push_back ( { serial, attached.contains ( serial ), true } );

	for ( const auto& serial : attached )
		if ( ! stored.contains ( serial ) )
			rows.push_back ( { serial, true, false } );

	list.updateContent ();
	list.repaint ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::moveSelected ( const int delta )
{
	const auto	row = list.getSelectedRow ();
	const auto	target = row + delta;

	if ( row < 0 || target < 0 || target >= int ( rows.size () ) )
		return;

	std::swap ( rows[ size_t ( row ) ], rows[ size_t ( target ) ] );

	list.updateContent ();
	list.selectRow ( target );
	list.repaint ();
	store ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsHardware::store ()
{
	juce::StringArray	chosen;

	for ( const auto& r : rows )
		if ( r.selected )
			chosen.add ( r.serial );

	preferences->set ( "hardware/boards", chosen.joinIntoString ( "," ) );

	msg::SettingChanged { "hardware", "boards" }.send ();
}
//-----------------------------------------------------------------------------

juce::String GUI_SettingsHardware::statusText () const
{
	if ( ! preferences->get<bool> ( "hardware/enabled" ) )
		return strings->get ( "settings/hardware/status-off" );

	const auto	s = hardware->status ();

	if ( ! s.open )
		return strings->get ( "settings/hardware/status-none" );

	auto	text = strings->get ( "settings/hardware/status-boards" ).replace ( "{}", boardsAndSids ( s ) );

	if ( s.detached )
		return text + "   " + strings->get ( "settings/hardware/status-detached" );

	if ( ! s.armed )
		return text + "   " + strings->get ( "settings/hardware/status-idle" );

	text << "   " << strings->get ( "settings/hardware/status-playing" )
					.replace ( "{released}", juce::String ( s.released ) )
					.replace ( "{dropped}", juce::String ( s.dropped ) )
					.replace ( "{lag}", juce::String ( s.lagMs, 1 ) );

	if ( s.unmappedChips > 0 )
		text << "   " << strings->get ( "settings/hardware/status-unmapped" ).replace ( "{}", juce::String ( s.unmappedChips ) );

	return text;
}
//-----------------------------------------------------------------------------
