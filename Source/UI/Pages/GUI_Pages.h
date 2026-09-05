#pragma once

#include <JuceHeader.h>

#include "App/PlayQueue.h"
#include "App/ThumbnailCache.h"
#include "UI/Components/PlaylistGrid/GUI_PlaylistGrid.h"

#include "CRT/GUI_CRT.h"
#include "Export/GUI_Export.h"
#include "History/GUI_History.h"
#include "Playlist/GUI_Playlist.h"
#include "Search/GUI_Search.h"
#include "Settings/GUI_Settings.h"

//-----------------------------------------------------------------------------

class GUI_Pages final : public juce::Component
{
public:
	GUI_Pages ( juce::AudioDeviceManager& adm );

	// juce::Component
	void resized () override;
	void paintOverChildren ( juce::Graphics& g ) override;

	// this
	void setPage ( const std::string& name );
	[[ nodiscard ]] std::string& getPage () noexcept { return currentPage; }

	[[ nodiscard ]] juce::String getSearchString ();
	[[ nodiscard ]] GUI_Results& getSearchResults () { return search.results; }

	void showSearch ();
	void showLiked ()	{	search.showLiked ();	}
	void setFileDatabase ( std::vector<const Database::entry*> fileDB );
	void setUserDatabase ( std::vector<const Database::entry*> userDB );

	// A user-tune file changed: the database freed/replaced its entries, so
	// every page holding cached entry pointers re-resolves them from its tune
	// keys. Playlist views go first, the grid tiles read from them
	void refreshUserTunes ();

	void setSearch ( const juce::String& searchStr, const bool notify );

	void showPlaylist ( const std::string& name );

	void loadHistory ();
	void loadExports ();
	void addToHistory ( const juce::String& tune, const int subtune );

	void setError ( const juce::String& error );
	void timerUpdate ( const float secondsPassed, uint16_t c64cpuCycles );

	[[ nodiscard ]] const std::vector<const Database::entry*>& getPlaylistEntries ( const std::string& name )
	{
		static const std::vector<const Database::entry*>	noEntries;

		const auto	items = playlist.getPlaylistItems ( name );

		return items ? items->getEntries () : noEntries;
	}

	void setPlaying ( const std::string& name, const int playlistPostion );

	void loadTune ( const juce::String&, const int subtune, const juce::String& src, const int playlistPostion );

	void setPlaylists ();
	void renamePlaylist ( const juce::String& oldName, const juce::String& newName );

	void updateItemInfo ();
	void repaintExportCell ( const int index );
	void updateGrid ()		{	playlistGrid.updateContent ();	}
	void updateGridItem ( const juce::String& name );

	[[ nodiscard ]] std::vector<const Database::entry*> getNonEmptyThumbnails ( const std::string& name, const int maxThumbs = 4 );

	[[ nodiscard ]] juce::StringArray getPlaylists () { return playlistNames; }

	void setCurrentPlaylist ( GUI_PlaylistItems* items );
	[[ nodiscard ]] GUI_PlaylistItems* getCurrentPlaylist ()								{	return currentPlaylist;		}
	[[ nodiscard ]] GUI_PlaylistItems* getPlaylistItems ( const juce::String& name ) 		{	return playlist.getPlaylistItems ( name );	}
	[[ nodiscard ]] GUI_PlaylistItems* getCurrentVisiblePlaylist ()	const					{	return playlist.currentVisible;	}

	// Playlist CRUD fan-out (see the "playlist" bus messages, Helpers/Messages.h)
	void playlistUpdated ( const juce::String& name )		{	playlistGrid.selectPlaylist ( name );	playlist.updatePlaylist ( name );	}
	void playlistAdded ( const juce::String& name )			{	playlist.addPlaylist ( name );		playlistGrid.addPlaylist ( name );	}
	void playlistDeleted ( const juce::String& name )		{	playlist.deletePlaylist ( name );	playlistGrid.removePlaylist ( name );	}
	void playlistInfoChanged ( const juce::String& name )	{	playlist.updateInfo ();				playlistGrid.updateGridItemByName ( name );	}

	// CRT page
	void loadGameArtwork ( const juce::String& sidName, const juce::String& index = "" )	{	crtPage.loadGameArtwork ( sidName, index );	}
	void loadGameArtwork ( const int index )				{	crtPage.loadGameArtwork ( index );	}
	void setTuneStrings ( const SidTuneInfoEZ& info )		{	crtPage.setStrings ( info );	}
	[[ nodiscard ]] bool isCRTVisible () const				{	return crtPage.isVisible ();	}
	[[ nodiscard ]] bool areCRTSettingsVisible () const		{	return crtPage.areSettingsVisible ();	}
	void showCRTSettings ( const bool visible )				{	crtPage.showSettings ( visible );	}
	[[ nodiscard ]] int getCRTPage () const					{	return crtPage.getCRTPage ();	}
	void setCRTPage ( const int page )						{	crtPage.setCRTPage ( page );	}
	void paintCRTIntoSnapshot ( juce::Image& snapshot, juce::Component& top )	{	crtPage.paintIntoSnapshot ( snapshot, top );	}
	[[ nodiscard ]] juce::File getLastLoadedArtwork ()		{	return crtPage.getLastLoadedFile ();	}
	void setCRTBackground ( const juce::Colour& col )		{	crtPage.setBackgroundColour ( col );	}
	void setCRTVoiceRegs ( const uint8_t* regs, const int count )	{	crtPage.setVoiceRegs ( regs, count );	}
	void setCRTPlaybackTime ( const int timeMS, const int lengthMS, const int renderMS )	{	crtPage.setPlaybackTime ( timeMS, lengthMS, renderMS );	}
	void playerLayoutChanged ()	{	crtPage.reloadPlayerLayout ();	}
	void bootScreenPickChanged ()	{	crtPage.bootScreenPickChanged ();	}
	void playerScreenPickChanged ()	{	crtPage.playerScreenPickChanged ();	}
	void reloadOverlayProfile ()							{	crtPage.reloadOverlayProfile ();	}
	void userCRTContentChanged ( const juce::String& relPath, const gin::FileSystemWatcher::FileSystemEvent event )	{	crtPage.userCRTContentChanged ( relPath, event );	}
	void userCRTPresetsChanged ()	{	crtPage.userCRTPresetsChanged ();	}

	// Export page
	void addExportItem ( const std::string& fullFilename, const std::string& item )	{	exportPage.addItem ( fullFilename, item );	}

	// Settings page
	void setHVSCStatus ( const GUI_SettingsLocationStatus::Status status, const juce::String& message )	{	settingsPage.setHVSCStatus ( status, message );	}
	void restoreSettingsPreferences ()	{	settingsPage.restorePreferences ();	}
	void refreshExportPreview ()		{	settingsPage.refreshExportPreview ();	}
	void setFFTSources ( const FFTMeasurement& left, const FFTMeasurement& right )	{	settingsPage.setFFTSources ( left, right );	}
	void spectrumChanged ( const bool stereo )	{	if ( settingsPage.isShowing () ) settingsPage.spectrumChanged ( stereo );	}

	// Search
	void repaintSearch ()	{	search.repaint ();	}

	GUI_Search			search;
	juce::Component	playlistGridWrapper { "playlistGridWrapper" };
		GUI_PlaylistGrid	playlistGrid;
	GUI_Playlist		playlist;
	GUI_History			history;
	GUI_CRT				crtPage;
	GUI_Export			exportPage;
	GUI_Settings		settingsPage;

private:
	gin::LayoutSupport	layout { *this };

	juce::SharedResourcePointer<ThumbnailCache>	thumbnailCache;
	juce::SharedResourcePointer<PlayQueue>		playQueue;

	std::string			currentPage;

	juce::StringArray	playlistNames;

	GUI_PlaylistItems*	currentPlaylist = nullptr;

	juce::String		error = "scanning";

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Pages )
};
//-----------------------------------------------------------------------------
