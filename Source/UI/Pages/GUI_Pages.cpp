#include <JuceHeader.h>

#include "GUI_Pages.h"

#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/ScreenshotLookup.h"
#include "Data/Tags.h"
#include "Database/Database.h"
#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

GUI_Pages::GUI_Pages ( juce::AudioDeviceManager& adm )
	: search ( *this )
	, playlistGrid ( *this, false )
	, playlist ( *this )
	, history ( *this )
	, settingsPage ( adm )
{
	setName ( "pages" );

	playlistGridWrapper.addAndMakeVisible ( playlistGrid );

	addChildComponent ( search );
	addChildComponent ( playlistGridWrapper );
	addChildComponent ( playlist );
	addChildComponent ( history );
	addChildComponent ( crtPage );
	addChildComponent ( exportPage );
	addChildComponent ( settingsPage );

	setPage ( "search" );
}
//-----------------------------------------------------------------------------

void GUI_Pages::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/pages.json" } );
}
//-----------------------------------------------------------------------------

void GUI_Pages::paintOverChildren ( juce::Graphics& g )
{
	if ( error == "" )
		return;

	if ( error == "scanning" )
	{
		GUI_LookAndFeel::drawRasterBars ( g, getLocalBounds ().toFloat () );
		return;
	}
}
//-----------------------------------------------------------------------------

void GUI_Pages::showSearch ()
{
	auto&	editor = search.searchbar.getTextEditor ();

	editor.grabKeyboardFocus ();
	editor.selectAll ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::setPage ( const std::string& name )
{
	currentPage = name;

	search.setVisible ( name == "search" );

	playlistGridWrapper.setVisible ( name == "playlists" && ! playlist.currentVisible );
	playlist.setVisible ( name == "playlists" && playlist.currentVisible );

	history.setVisible ( name == "history" );

	crtPage.setVisible ( name == "crt" );
	exportPage.setVisible ( name == "export" );
	settingsPage.setVisible ( name == "settings" );
}
//-----------------------------------------------------------------------------

juce::String GUI_Pages::getSearchString ()
{
	return search.searchbar.getTextEditor ().getText ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::setFileDatabase ( std::vector<const Database::entry*> fileDB )
{
	search.setDatabase ( std::move ( fileDB ) );
	search.updateSearch ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::setUserDatabase ( std::vector<const Database::entry*> userDB )
{
	search.setUserDatabase ( std::move ( userDB ) );
	search.updateSearch ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::refreshUserTunes ()
{
	playlist.refreshRowData ();
	playlistGrid.updateContent ();
	history.refreshRowData ();
	exportPage.refreshRowData ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::setSearch ( const juce::String& searchStr, const bool notify )
{
	search.search ( searchStr );

	if ( notify )
		msg::ShowSearch {}.send ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::setPlaylists ()
{
	const juce::SharedResourcePointer<Playlists> playlists;

	auto	playlistNames = playlists->getPlaylistNames ();

	// Detach before the views are destroyed, the queue's position points into them
	setCurrentPlaylist ( nullptr );

	playlist.setPlaylists ( playlistNames );
	playlistGrid.setPlaylists ( playlistNames );
}
//-----------------------------------------------------------------------------

void GUI_Pages::renamePlaylist ( const juce::String& oldName, const juce::String& newName )
{
	const auto	renamedTheCurrent = currentPlaylist && currentPlaylist->getName () == oldName;

	playlist.renamePlaylist ( oldName.toStdString (), newName.toStdString () );
	playlistGrid.removePlaylist ( oldName );
	playlistGrid.addPlaylist ( newName );

	// Same view under a new name, so the queue follows it
	if ( renamedTheCurrent )
		setCurrentPlaylist ( currentPlaylist );
}
//-----------------------------------------------------------------------------

void GUI_Pages::updateItemInfo ()
{
	playlist.updateInfo ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::repaintExportCell ( const int index )
{
	exportPage.repaintCell ( index );
}
//-----------------------------------------------------------------------------

void GUI_Pages::updateGridItem ( const juce::String& name )
{
	playlistGrid.updateGridItemByName ( name );
	playlist.updateInfo ();
}
//-----------------------------------------------------------------------------

std::vector<const Database::entry*> GUI_Pages::getNonEmptyThumbnails ( const std::string& name, const int maxThumbs/* = 4 */)
{
	// Add database-entries to grid-item
	const juce::SharedResourcePointer<ScreenshotLookup>	scrSht;

	std::vector<const Database::entry*>	images;

	for ( auto entry : getPlaylistEntries ( name ) )
	{
		if ( ! entry )	// Missing tune, no thumbnail to show
			continue;

		if ( images.size () >= size_t ( maxThumbs ) )
			continue;

		if ( auto str = scrSht->getDefaultScreenshot ( std::string ( entry->file ) ); str.empty () )
			continue;

		if ( std::ranges::find ( images, entry ) != images.end () )
			continue;

		images.push_back ( entry );
	}

	return images;
}
//-----------------------------------------------------------------------------

void GUI_Pages::setCurrentPlaylist ( GUI_PlaylistItems* items )
{
	if ( currentPlaylist && items != currentPlaylist )
		currentPlaylist->setPlayingRow ( -1 );

	currentPlaylist = items;

	// Keep the playback queue following the same playlist
	playQueue->setPlaylist ( items ? items->getName ().toStdString () : std::string () );
}
//-----------------------------------------------------------------------------

void GUI_Pages::showPlaylist ( const std::string& name )
{
	playlist.showPlaylist ( name );
	setPage ( "playlists" );
}
//-----------------------------------------------------------------------------

void GUI_Pages::loadHistory ()
{
	history.load ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::loadExports ()
{
	exportPage.load ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::addToHistory ( const juce::String& tune, const int subtune )
{
	history.addItem ( tune.toStdString (), subtune );
}
//-----------------------------------------------------------------------------

void GUI_Pages::setError ( const juce::String& _error )
{
	error = _error;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::timerUpdate ( const float secondsPassed, uint16_t c64cpuCycles )
{
	if ( error == "scanning" )
	{
		repaint ();
		return;
	}

	search.results.timerUpdate ( secondsPassed );

	if ( playlist.currentVisible )
		playlist.currentVisible->timerUpdate ( secondsPassed );

	crtPage.timerUpdate ( secondsPassed, c64cpuCycles );
	exportPage.update ();
}
//-----------------------------------------------------------------------------

void GUI_Pages::setPlaying ( const std::string& name, const int playlistPostion )
{
	if ( currentPlaylist )
		currentPlaylist->setPlayingRow ( playlistPostion );

	search.results.setPlayingName ( playlistPostion < 0 ? name : "" );
}
//-----------------------------------------------------------------------------

void GUI_Pages::loadTune ( const juce::String& name, const int subtune, const juce::String& src, const int playlistPostion )
{
	msg::LoadTune { name, subtune, src, playlistPostion }.send ();
}
//-----------------------------------------------------------------------------
