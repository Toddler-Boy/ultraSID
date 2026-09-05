#include "std_lime/lime_string_utils.h"

#include "Config/FilePaths.h"
#include "Database/Database.h"
#include "Database/TuneInfo.h"
#include "Database/UserLoudness.h"
#include "Helpers/Messages.h"

#include "GUI_ChipProfileEditor.h"
#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

void GUI_ultraSID::updateTransportButtons ()
{
	const auto	inPlaylist = playQueue->isActive ();
	const auto	hasQueue = inPlaylist && playQueue->getSize () > 0;

	const auto	renderLength = player.getRenderLength ();

	mainScreen.footer.updateTransport ( player.isPaused (),
										renderLength > 0,
										hasQueue,
										hasQueue,
										inPlaylist,
										renderLength );
}
//-----------------------------------------------------------------------------

// A real file's absolute path, a "$HVSC$/..." key for zip-backed collection
// tunes, a data-relative path for pak-backed tunes (the Exotic mirror), or
// "" when the tune is nowhere to be found
std::string GUI_ultraSID::getFullFilename ( const juce::String& filename )
{
	if ( ! areRootsValid () )
		return {};

	return filepaths::resolveTune ( filename ).toLoadable ().toStdString ();
}
//-----------------------------------------------------------------------------

bool GUI_ultraSID::loadSong ( const juce::String& filename )
{
	lastFilename = "";

	auto	fullName = getFullFilename ( filename );
	if ( fullName.empty () )
		return false;

	lastFilename = filename.toStdString ();
	return player.loadTune ( fullName );
}
//-----------------------------------------------------------------------------

bool GUI_ultraSID::initSong ( unsigned int songNum, const bool subTuneOnly )
{
	if ( lastFilename.empty () )
		return false;

	disableAudio ();

	if ( ! songNum )
		songNum = db::findDatabaseEntry ( lastFilename )->startTune;

	const auto	ready = player.init ( songNum, database->getSongFilterUsed ( lastFilename, songNum ) );

	lastSong = player.getCurrentSong ();

	//
	// Get basic information about tune
	//
	const auto	str = player.getFileInfo ();

	jassert ( juce::MessageManager::getInstance ()->isThisTheMessageThread () );

	// Show chips, FFTs & frequency lines
	lastSidDataCount = 0;
	fftMeasureLeft.reset ();
	fftMeasureRight.reset ();
	mainScreen.sidebarRight.showTune ( str, player.getNumChips (), player.isNTSC (), database->getSongDigiUsed ( lastFilename, lastSong ) );

	player.setDacLeakage ( preferences->get<bool> ( "emulation/dac-leakage" ) * 1.0 );

	if ( ! subTuneOnly )
	{
		mainScreen.sidebarRight.showMemoryOverview ( str );

		//
		// Tell Info section about this file
		//
		mainScreen.footer.showTuneInfo ( str );
		mainScreen.pages.setTuneStrings ( str );

		//
		// Tell effects how many channels to process
		//
		dspEffects.setChipModel ( ! str.model.empty () && str.model[ 0 ] == "6581", str.clock == "NTSC", str.stereoWidth, str.bassAdjust );
	}
	dspEffects.clearClipIndicators ();

	// The chip-profile editor follows the tune (fresh values + fresh push); a
	// live-tweak restart of the same tune keeps the tweaked values instead
	if ( ready && chipEditor != nullptr && ! inTweakRestart )
		chipEditor->refresh ( currentChipSettings () );

	enableAudio ();

	return ready;
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::renderSong ()
{
	const auto [ len, songFade, lufs, filterUsed, skipMs ] = SID::getRenderInfo ( lastFilename, player.getCurrentSong () );

	const auto	isUserTune = lastFilename.starts_with ( filepaths::userMarker );
	const auto	unknown = lufs >= -0.1f || lufs <= -95.9f;

	if ( unknown )
	{
		Z_INFO ( "Unknown loudness, measuring live" );
	}
	else
	{
		// The rating already contains the midband punishment, the raw stored
		// loudness recovers it for display
		const auto	raw = isUserTune
						? juce::SharedResourcePointer<UserLoudness> ()->get ( UserLoudness::keyFor ( lastFilename ), player.getCurrentSong () ).first
						: juce::SharedResourcePointer<Database> ()->getSongLoudness ( lastFilename, player.getCurrentSong () );
		const auto	punishment = lufs - raw;

		auto	msg = "Replay gain " + juce::String ( std::min ( 20.0f, SIDPlayer::targetLUFS - lufs ), 1 ) + " dB";
		if ( punishment > 0.05f )
			msg << " (midband punishment " << juce::String ( punishment, 1 ) << " dB)";

		Z_INFO ( msg );
	}

	// A completed live measurement of a user tune goes into the cache, keyed
	// to the tune this render was started for
	if ( isUserTune && unknown )
	{
		player.onLoudnessMeasured = [ key = UserLoudness::keyFor ( lastFilename ), song = player.getCurrentSong () ] ( const float integrated, const float mid )
		{
			juce::MessageManager::callAsync ( [ = ]
			{
				const juce::SharedResourcePointer<UserLoudness>	userLoudness;
				userLoudness->store ( key, song, integrated, mid );
			} );
		};
	}
	else
		player.onLoudnessMeasured = nullptr;

	// Without a render thread the tune neither plays nor ever finishes
	if ( ! player.startRender ( len, songFade, lufs, skipMs ) )
		Z_ERR ( "Could not start rendering " << lastFilename << " (song " << int ( player.getCurrentSong () ) << "): "
				<< ( player.isReadyToPlay () ? "the render thread would not start" : "tune reports not ready to play" ) );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::togglePause ()
{
	if ( ! player.isReadyToPlay () )
		initSong ( lastSong, false );
	else
		player.togglePlayPause ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updatePlaylistPosition ()
{
	//
	// Called once per v-blank to check if the song
	// is over and move on to next one in playlist
	//
	if ( ! player.finishedPlaying () )
		return;

	// Live-tweak mode never advances the playlist: the next tune (possibly by
	// a different author) would re-load its profile into the editor and wipe
	// the tweaks; loop the current song instead
	if ( chipEditor != nullptr )
	{
		restartTweakRender ( 0 );
		return;
	}

	// Repeat "one"
	if ( getRepeatMode () == PlayQueue::Repeat::one )
	{
		player.seek ( 0 );
		return;
	}

	if ( playQueue->position < 0 )
	{
		mainScreen.pages.setPlaying ( "", -1 );
		mainScreen.sidebarRight.setTunePlaying ( -1 );
		return;
	}

	nextPreviousPlaylistItem ( 1, false );
}
//-----------------------------------------------------------------------------

PlayQueue::Repeat GUI_ultraSID::getRepeatMode () const
{
	switch ( mainScreen.footer.getRepeatStage () )
	{
		case GUI_TransportButton::REPEAT_ONE:	return PlayQueue::Repeat::one;
		case GUI_TransportButton::REPEAT_ALL:	return PlayQueue::Repeat::all;
		default:								return PlayQueue::Repeat::off;
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::nextPreviousPlaylistItem ( const int delta, const bool manual )
{
	const auto	next = playQueue->advance ( delta,
											getRepeatMode (),
											mainScreen.footer.isShuffleOn (),
											manual );

	if ( next.stopped )
	{
		mainScreen.pages.setPlaying ( "", -1 );
		mainScreen.sidebarRight.setTunePlaying ( -1 );
		return;
	}

	if ( ! next.entry )
		return;

	loadTune ( juce::String ( next.entry->file.data (), next.entry->file.size () ), next.subtune, "playlist-next/prev", next.playPosition );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::loadTune ( const juce::String& name, const int subtune, const juce::String& src, const int _playlistPosition )
{
	// An unplayable tune must not park the playlist on itself. The counter bounds a
	// playlist of broken tunes; only a tune that actually plays clears it
	auto	skipBroken = [ this ] ( const juce::String& why )
	{
		Z_ERR ( why );

		if ( ++brokenTuneSkips > maxBrokenTuneSkips )
		{
			Z_ERR ( "Giving up after " << maxBrokenTuneSkips << " unplayable tunes in a row" );
			return;
		}

		nextPreviousPlaylistItem ( 1, false );
	};

	if ( getFullFilename ( name ).empty () )
		return skipBroken ( "Tune file is gone: " + name );

	// Must fold exactly like the database builds lowerFile (ASCII-only lime toLower)
	mainScreen.pages.setPlaying ( lime::str::toLower ( name.toStdString () ), _playlistPosition );
	mainScreen.sidebarRight.setTunePlaying ( -1 );

	// A queue advance already computed the logical position, which differs from
	// the played row under shuffle; a hand-picked row defines both
	if ( src != "playlist-next/prev" )
		playQueue->position = _playlistPosition;

	playQueue->subtune = subtune;

	disableAudio ();

	player.stopRender ();

	if ( ! loadSong ( name ) )
	{
		enableAudio ();
		return skipBroken ( "Can't load " + name );
	}

	if ( ! initSong ( subtune, false ) )
	{
		enableAudio ();
		return skipBroken ( "Can't initialize " + juce::String ( lastFilename ) );
	}

	renderSong ();
	dspEffects.setStereo ( player.getNumChips () > 1 );

	enableAudio ();

	brokenTuneSkips = 0;

	lastTuneSrc = src.toStdString ();
	lastTuneList = lastTuneSrc.starts_with ( "playlist" ) ? playQueue->getPlaylistName () : std::string ();

	updateTransportButtons ();

	auto	sidTuneInfo = player.getFileInfo ();

	// Update STIL
	{
		preProcessSTIL ( name, sidTuneInfo.startSong );

		for ( auto tuneNo = 1u; tuneNo <= sidTuneInfo.numSongs; ++tuneNo )
			mainScreen.sidebarRight.setTuneLength ( tuneNo, SID::getTuneLength ( lastFilename, tuneNo ) );

		mainScreen.sidebarRight.setDefaultTune ( sidTuneInfo.title, sidTuneInfo.startSong );
		mainScreen.sidebarRight.setTunePlaying ( lastSong );
		mainScreen.sidebarRight.updateFilterStates ();
	}

	// Update footer thumbnail
	updateFooterThumbnail ( name.toStdString () );

	// Set CRT page to new artwork
	mainScreen.pages.loadGameArtwork ( name );

	// Add to history
	if ( src != "history" && src != "restore" && src != "playlist-next/prev" )
		mainScreen.pages.addToHistory ( name, subtune );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toggleLikePlaying ()
{
	if ( lastFilename.empty () )
		return;

	likes->toggle ( lastFilename, int ( lastSong ) );

	msg::LikeChanged { juce::String ( lastFilename ), int ( lastSong ) }.send ();
}
//-----------------------------------------------------------------------------

// Switch to the page the playing tune was started from and select its row,
// resetting search and filters when needed to find it
void GUI_ultraSID::jumpToPlayingTune ()
{
	if ( lastFilename.empty () || lastTuneSrc.empty () )
		return;

	const auto	lowerFile = lime::str::toLower ( lastFilename );
	const auto	subtune = int ( lastSong );

	auto&	pages = mainScreen.pages;

	if ( ! lastTuneList.empty () )
	{
		showPlaylist ( lastTuneList );

		if ( auto items = pages.getPlaylistItems ( lastTuneList ) )
		{
			// The queue row is exact only while the queue still follows this playlist
			if ( playQueue->getPlaylistName () == lastTuneList && playQueue->playPosition >= 0 )
				items->showRow ( playQueue->playPosition );
			else
				items->showRow ( items->findRow ( lowerFile, subtune ) );
		}
	}
	else if ( lastTuneSrc == "history" )
	{
		lastPage = "history";
		showPage ( lastPage );
		pages.history.showTune ( lowerFile, subtune );
	}
	else if ( lastTuneSrc == "export" )
	{
		lastPage = "export";
		showPage ( lastPage );
		pages.exportPage.showTune ( lowerFile, subtune );
	}
	else	// search, STIL and anything unexpected
	{
		lastPage = "search";
		showPage ( lastPage );

		auto&	results = pages.getSearchResults ();

		auto	row = results.findRow ( lowerFile, subtune );
		if ( row < 0 )
		{
			pages.search.clearFilters ();
			pages.setSearch ( "", false );

			// The header re-sorts asynchronously, the row lookup must run
			// after that pass
			juce::MessageManager::callAsync ( [ safe = juce::Component::SafePointer<GUI_Results> ( &results ), lowerFile, subtune ]
			{
				if ( safe != nullptr )
					safe->showRow ( safe->findRow ( lowerFile, subtune ) );
			} );

			return;
		}

		results.showRow ( row );
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::playSubtune ( const int subtune )
{
	// A manually picked subtune (the STIL list is the only caller) detaches from
	// the playlist: it plays once and stops, like a tune started from the search
	playQueue->position = -1;

	if ( playQueue->subtune == subtune )
	{
		player.seek ( 0 );
		return;
	}

	playQueue->subtune = subtune;

	disableAudio ();

	player.stopRender ();

	if ( ! initSong ( (unsigned int)subtune, true ) )
	{
		Z_ERR ( "Can't initialize " << lastFilename );
		enableAudio ();
		return;
	}

	renderSong ();

	enableAudio ();

	updateTransportButtons ();
	mainScreen.sidebarRight.setTunePlaying ( int ( lastSong ) );
}
//-----------------------------------------------------------------------------

// Playlist CRUD fan-out, the UI reactions to playlist changes; the data
// itself is already updated by the sender (Data/Playlists)
void GUI_ultraSID::registerPlaylistActions ()
{
	// "update" and "addTo" trigger the same refresh; scrolling to the
	// playlist comes first, a fresh badge needs its tile on screen
	const auto	updated = [ this ] ( const auto& e )
	{
		mainScreen.sidebarLeft.selectPlaylist ( e.name );

		mainScreen.pages.playlistUpdated ( e.name );
		mainScreen.sidebarLeft.updatePlaylistItem ( e.name );
	};

	router.on<msg::PlaylistUpdate> ( updated );
	router.on<msg::PlaylistAddTo> ( updated );

	router.on<msg::PlaylistNew> ( [ this ] ( const auto& e )
	{
		mainScreen.pages.playlistAdded ( e.name );

		mainScreen.sidebarLeft.addPlaylist ( e.name );
		mainScreen.sidebarLeft.updatePlaylistItem ( e.name );
	} );

	router.on<msg::PlaylistDeleteList> ( [ this ] ( const auto& e )
	{
		mainScreen.pages.playlistDeleted ( e.name );
		mainScreen.sidebarLeft.removePlaylist ( e.name );
	} );

	router.on<msg::PlaylistRenamed> ( [ this ] ( const auto& e )
	{
		mainScreen.sidebarLeft.removePlaylist ( e.oldName );
		mainScreen.sidebarLeft.addPlaylist ( e.newName );
		mainScreen.sidebarLeft.refreshPlaylists ();
		mainScreen.sidebarLeft.selectPlaylist ( e.newName );
	} );

	router.on<msg::PlaylistUpdateInfo> ( [ this ] ( const auto& e )
	{
		mainScreen.pages.playlistInfoChanged ( e.name );
		mainScreen.sidebarLeft.updatePlaylistItem ( e.name );
	} );

	router.on<msg::BadgeSpawn> ( [ this ] ( const auto& e )
	{
		badgeOverlay.spawn ( badgeOverlay.getLocalPoint ( nullptr, juce::Point<int> ( e.x, e.y ) ), e.delta );
	} );
}
//-----------------------------------------------------------------------------
