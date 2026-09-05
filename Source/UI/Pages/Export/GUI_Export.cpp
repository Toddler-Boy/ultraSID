#include <JuceHeader.h>

#include "GUI_Export.h"

#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/ExportName.h"
#include "Audio/SIDEffects.h"
#include "Database/Database.h"
#include "Database/TuneInfo.h"
#include "UI/UI_Menus.h"

//-----------------------------------------------------------------------------

GUI_Export::GUI_Export ()
{
	setName ( "export" );

	label.setName ( "header" );

	menuButton.onClick = [ this ] { showMenu (); };

	addAndMakeVisible ( label );
	addAndMakeVisible ( menuButton );
	addAndMakeVisible ( exportItems );
}
//-----------------------------------------------------------------------------

void GUI_Export::showMenu ()
{
	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	auto	m = UI::newPopupMenu ( *this );

	// Put the canceled entries back into the queue
	m.addItem ( UI::newMenuItem ( strings->get ( "menu/export_canceled" ), icons->get ( "menu/requeue_export" ), [ this ]
	{
		exportItems.reAddCanceled ();
	} ).setEnabled ( exportItems.hasCanceled () ) );

	m.addSeparator ();

	UI::menu_ClearOlderThan ( m, [ this ] ( double days ) { exportItems.clearOlderThan ( days ); } );

	// Clear all
	m.addSeparator ();

	m.addItem ( UI::newDangerousMenuItem ( strings->get ( "menu/clear_all" ), icons->get ( "menu/delete" ), [ this ]
	{
		exportItems.clearAll ();
	} ) );

	UI::showMenuAtButton ( m, *this, menuButton );
}
//-----------------------------------------------------------------------------

void GUI_Export::update ()
{
	if ( isShowing () )
		exportItems.update ();
}
//-----------------------------------------------------------------------------

void GUI_Export::addItem ( const std::string& filename, const std::string& _tuneName )
{
	auto [ tuneName, subtune ] = SID::parseTuneName ( _tuneName );

	const juce::ScopedLock	sl ( exportLock );

	auto	ent = db::findDatabaseEntry ( tuneName );
	if ( ! ent )
	{
		Z_ERR ( "Can't find " << tuneName << " in database" );
		return;
	}

	// If tune is default, find number of start-tune
	if ( subtune == 0 )
		subtune = ent->startTune;

	const auto	quality = preferences->get<juce::String> ( "player/quality" );

	const auto	saveName = exportname::makeExportPath ( *ent, subtune, quality.toStdString () );
	if ( saveName.empty () )
		return;

	const auto	[ lengthMs, fadeMs, ebuGain, filterUsed, startMs ] = SID::getRenderInfo ( tuneName, subtune );

	// Exports follow the footer quality, captured when the tune is queued
	exportItems.addItem ( tuneName, { filename, subtune, int ( lengthMs ), int ( fadeMs ), int ( startMs ), ebuGain, fxModeForQuality ( quality ), filterUsed, preferences->get<bool> ( "export/normalize" ), saveName } );
}
//-----------------------------------------------------------------------------

int GUI_Export::fxModeForQuality ( const juce::String& quality )
{
	if ( quality.equalsIgnoreCase ( "REAL" ) )			return SIDEffects::FXMode::REAL;
	if ( quality.equalsIgnoreCase ( "MAGIC" ) )			return SIDEffects::FXMode::MAGIC;
	if ( quality.equalsIgnoreCase ( "EPIC" ) )			return SIDEffects::FXMode::EPIC;
	if ( quality.equalsIgnoreCase ( "MYTHIC" ) )		return SIDEffects::FXMode::MYTHIC;

	if ( ! quality.equalsIgnoreCase ( "PURE" ) )
		Z_ERR ( "Unknown export quality: " << quality );

	return SIDEffects::FXMode::PURE;
}
//-----------------------------------------------------------------------------
