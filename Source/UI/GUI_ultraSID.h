#pragma once

#include <JuceHeader.h>

#include "ultra-shared/App/AppUpdater.h"
#include "ultra-shared/Helpers/MessageRouter.h"
#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Shortcuts.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/Resources/Theme.h"
#include "ultra-shared/UI/Components/GUI_FocusRing.h"
#include "ultra-shared/UI/Components/GUI_TooltipWindow.h"
#include "ultra-shared/UI/GUI_About.h"
#include "ultra-shared/UI/GUI_Shortcuts.h"

#include "App/AppRoots.h"
#include "App/HVSCInstaller.h"
#include "App/InstallState.h"
#include "App/PlayQueue.h"
#include "App/ScreenshotLookup.h"
#include "App/ThumbnailCache.h"
#include "App/UndoManager.h"
#include "Audio/SIDEffects.h"
#include "Audio/SIDPlayer.h"
#include "Config/Preferences.h"
#include "Config/Settings.h"
#include "Data/History.h"
#include "Data/Likes.h"
#include "Data/Playlists.h"
#include "Data/Tags.h"
#include "Database/Database.h"
#include "Database/HVSCDatabase.h"
#include "Resources/STIL_Lookup.h"
#include "UI/Components/Badge/GUI_ultraSID_badge.h"
#include "UI/Components/FFT/FFTMeasurement.h"
#include "UI/Components/Footer/GUI_Footer.h"
#include "UI/Components/GUI_BadgeOverlay.h"
#include "UI/Components/GUI_LevelMeter.h"
#include "UI/Components/GUI_UndoToast.h"
#include "UI/GUI_SidebarLeft.h"
#include "UI/GUI_SidebarRight.h"
#include "UI/Screens/GUI_InstallScreen.h"
#include "UI/Screens/GUI_Main.h"

#if ULTRA_INSPECTOR
	#include <melatonin_inspector/melatonin_inspector.h>
#endif

class GUI_ChipProfileEditor;
class GUI_ColorAdjust;
class SharedProfiles;

//-----------------------------------------------------------------------------

class GUI_ultraSID final
	: public juce::AudioAppComponent
	, public juce::DragAndDropContainer
	, public juce::FileDragAndDropTarget
	, public juce::TextDragAndDropTarget
	, public juce::ActionBroadcaster
	, private gin::FileSystemWatcher::Listener
	, private juce::ActionListener
	, private juce::FocusChangeListener
{
public:
	GUI_ultraSID ();
	~GUI_ultraSID () override;

	// juce::AudioAppComponent
	void prepareToPlay ( int samplesPerBlockExpected, double sampleRate ) override;
	void getNextAudioBlock ( const juce::AudioSourceChannelInfo& bufferToFill ) override;
	void releaseResources () override;

	// juce::Component
	void moved () override;
	void resized () override;
	void parentHierarchyChanged () override;

	// juce::MouseListener
	void mouseDoubleClick ( const juce::MouseEvent& evt ) override;
	void mouseDown ( const juce::MouseEvent& evt ) override;

	// juce::VBlankAttachment
	void update ( double time );

	// this
	void setHVSCRoot ();
	void setDataRoot ();
	void setUserRoot ();

	[[ nodiscard ]] bool isHVSCRootValid () const
	{
		// A previous install never finished, the spot checks can't be trusted
		if ( settings->get<bool> ( "hvsc/install-in-progress" ) )
			return false;

		return roots.isHVSCValid ();
	}
	[[ nodiscard ]] bool areRootsValid () const		{	return roots.isHVSCValid ();	}

	void loadDatabase ();
	void loadROMs ();
	void preProcessSTIL ( const juce::String& filename, const unsigned int mainSong );

	// Error messages
	void checkHVSCStatus ();
	void setHVSCStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message );
	void checkDatabaseStatus ();

	// A failure must never strand the user on an install progress page
	void resetInstallScreens ();

	// Playlist/transport
	void loadTune ( const juce::String& name, const int subtune, const juce::String& src, const int playlistPosition = -1 );
	[[ nodiscard ]] bool loadSong ( const juce::String& filename );
	bool initSong ( unsigned int songNum, const bool subTuneOnly );
	void renderSong ();

	void togglePause ();

	// App state
	void saveState ();
	void restoreState ();

	// Commit a pending undo and stop background work before quitting
	void prepareToQuit ()	{	undoManager->flush ();	hvscInstaller.stop ();	}

	// juce::FileDragAndDropTarget
	bool isInterestedInFileDrag ( const juce::StringArray& files ) override;
	void filesDropped ( const juce::StringArray& files, int x, int y ) override;

	// juce::TextDragAndDropTarget
	bool isInterestedInTextDrag ( const juce::String& text ) override;
	void textDropped ( const juce::String& text, int x, int y ) override;

private:
	// The app's own device manager, so initAudio () can set rate and block size.
	// Stays first, it has to outlive the device selector
	juce::AudioDeviceManager	ownedDeviceManager;

	gin::LayoutSupport	layout { *this };
	std::once_flag		firstStart;

	// V-blank stuff
	juce::VBlankAttachment	vBlankAttachment { this, [ this ] ( double time )	{	update ( time );	} };
	double	lastTimer = 0.0;

	// juce::ActionListener
	void actionListenerCallback ( const juce::String& message ) override;

	// Message-bus routing; handlers are registered by role, each group in the
	// partial file that owns that role (see GUI_ultraSID_router.cpp)
	msg::Router	router;
	void registerActions ();
	void registerNavigationActions ();		// GUI_ultraSID_router.cpp
	void registerTransportActions ();		// GUI_ultraSID_router.cpp
	void registerSettingsActions ();		// GUI_ultraSID_router.cpp
	void registerArtworkActions ();			// GUI_ultraSID_router.cpp
	void registerShortcutActions ();		// GUI_ultraSID_key_shortcuts.cpp
	void registerPlaylistActions ();		// GUI_ultraSID_playlist.cpp
	void registerDownloadActions ();		// GUI_ultraSID_downloads.cpp
	void registerExportActions ();			// GUI_ultraSID_downloads.cpp

	// gin::FileSystemWatcher::Listener
	void fileChanged ( const juce::File& file, gin::FileSystemWatcher::FileSystemEvent event ) override;

	// juce::FocusChangeListener: feeds the focus ring, logs in developer mode
	void globalFocusChanged ( juce::Component* focused ) override;

	// juce::Component
	bool keyPressed ( const juce::KeyPress& key ) override;

	// this
	std::string		lastPage;
	bool			settingsAreVisible = false;

	void toggleFullscreen ();
	bool wasFullscreen = false;
	[[ nodiscard ]] bool isFullscreen () const;
	void toFullscreen ();
	void toWindowed ();

	[[ nodiscard ]] std::string getFullFilename ( const juce::String& filename );

	void showPage ( const std::string& name );
	void showPlaylist ( const std::string& name );
	void jumpToPlayingTune ();
	void toggleLikePlaying ();
	void updateTransportButtons ();
	[[ nodiscard ]] bool isOnboardingOrUpdating () const { return onboardingScreen.isVisible () || updateHVSCScreen.isVisible (); }
	void loadTheme ();
	void loadSIDPlayerProfilesAndOverrides ();

	void updateVoices ();

	// Register history count at the last chip-display feed (player reports >= 1)
	int		lastSidDataCount = 0;

	void updateVolume ();
	void updateFooterThumbnail ( const std::string& tunename );
	void updateColors ();

	void updateFX ();
	void updateUserEQ ();

	void applyPreferences ();

	juce::CriticalSection	inAudio;
	std::atomic<int>		muted = 0;
	SmoothedValue			curOutVol;
	void initAudio ();
	void disableAudio ();
	void enableAudio ();

	unsigned int	lastSong = 0;
	std::string		lastFilename;

	// Source of the playing tune: the loadTune src tag, plus the playlist name
	// when it came from one (the queue itself follows the visible playlist)
	std::string		lastTuneSrc;
	std::string		lastTuneList;

	// Consecutive unplayable tunes auto-skipped; bounds an all-broken playlist
	static constexpr int	maxBrokenTuneSkips = 10;
	int						brokenTuneSkips = 0;

	// Playlist stuff
	juce::SharedResourcePointer<PlayQueue>	playQueue;
	void updatePlaylistPosition ();
	void nextPreviousPlaylistItem ( const int delta, const bool manual );
	[[ nodiscard ]] PlayQueue::Repeat getRepeatMode () const;

	// Sub-tunes
	void playSubtune ( const int subtune );

	// Chip-profile editor (GUI_ultraSID_chip_editor.cpp): the hidden real-time
	// tweak window for sid-authors
	void toggleChipProfileEditor ();
	void closeChipProfileEditor ();
	void restartTweakRender ( uint32_t resumeMS );
	[[ nodiscard ]] SIDPlayer::ChipSettings currentChipSettings () const;

	std::unique_ptr<GUI_ChipProfileEditor>	chipEditor;
	bool	inTweakRestart = false;

	// Color-adjustment window: the hidden UI color grading (gamma, brightness,
	// contrast, saturation) applied to every themed color
	void toggleColorAdjust ();
	void updateColorAdjustments ();
	[[ nodiscard ]] Theme::ColorAdjustments colorAdjustmentsFromPreferences () const;

	std::unique_ptr<GUI_ColorAdjust>	colorAdjust;

	// Downloading
	void startFullInstall ();
	void downloadScreenshot ( const juce::String& dlUrl );

	// The first URL that answers becomes the cover, the rest are fallbacks
	void downloadCoverImage ( const juce::String& targetPlaylist, juce::StringArray dlUrls );

	// An M3U becomes a new playlist, a sibling .jpg/.png of the same name its cover
	void downloadPlaylist ( const juce::URL& dlUrl );

	// Global objects
	juce::SharedResourcePointer<Icons>			icons;
	juce::SharedResourcePointer<InstallState>	installState;
	juce::SharedResourcePointer<Likes>			likes;
	juce::SharedResourcePointer<History>		history;
	juce::SharedResourcePointer<Playlists>		playlists;
	juce::SharedResourcePointer<Preferences>	preferences;
	juce::SharedResourcePointer<Settings>		settings;
	juce::SharedResourcePointer<Shortcuts>		shortcuts;
	juce::SharedResourcePointer<Strings>		strings;
	juce::SharedResourcePointer<Tags>			tags;
	juce::SharedResourcePointer<Theme>			theme;

	AppRoots	roots;

	juce::SharedResourcePointer<HVSC_database>		hvscDatabase;
	juce::SharedResourcePointer<Database>			database;
	juce::SharedResourcePointer<UserDatabase>		userDatabase;
	juce::SharedResourcePointer<ThumbnailCache>		thumbnailCache;
	juce::SharedResourcePointer<ScreenshotLookup>	screenshots;

	juce::SharedResourcePointer<SharedProfiles>		profiles;
	juce::SharedResourcePointer<STILLookup>			stilLookup;

	SIDPlayer		player;
	SIDEffects		dspEffects;

	// One measurement per channel, fed from the audio callback and driving
	// both the sidebar FFT curves and the footer spectrum
	FFTMeasurement	fftMeasureLeft, fftMeasureRight;

	GUI_Main			mainScreen;
	GUI_InstallScreen	onboardingScreen { GUI_InstallScreen::Mode::onboarding };
	GUI_InstallScreen	updateHVSCScreen { GUI_InstallScreen::Mode::update };
	GUI_About			aboutScreen;
	GUI_Shortcuts		shortcutsScreen;

	GUI_LevelMeter		inputMeter[ 2 ] = { dspEffects.inputLevel[ 0 ], dspEffects.inputLevel[ 1 ] };
	GUI_LevelMeter		outputMeter[ 2 ] = { dspEffects.outputLevel[ 0 ], dspEffects.outputLevel[ 1 ] };

	gin::FileSystemWatcher	folderWatcher;

	// The SID engine's rate
	static constexpr auto	internalSamplerate = 44100;

	// 60Hz worth of audio-data (technically only 44100 / 60 are needed, but better safe than sorry)
	int	sampleRate = 0;
	juce::AudioBuffer<float>	sidBuffer;
    gin::ResamplingFifo			resamplingFifo { 1024 * 5 };

	juce::SharedResourcePointer<GUI_TooltipWindow>	tooltipWindow;

	juce::SharedResourcePointer<UndoManager>	undoManager;
	GUI_UndoToast	undoToast;
	void positionUndoToast ();

	GUI_BadgeOverlay	badgeOverlay;
	GUI_FocusRing		focusRing;

	// Work count at the last export-badge update, user deltas badge from it
	int	lastExportWork = 0;

	void addScreenshots ( const juce::StringArray& filenames );
	void addSidTunes ( const juce::StringArray& filenames );

	// Each M3U becomes a new playlist, a .jpg/.png beside it its cover
	void addPlaylistFiles ( const juce::StringArray& filenames );

	void assignBorderColor ( const int index );
	void toggleFirstLuma ();
	void toggleFirstLumaAll ();
	void toggleThumbnail ();
	void deleteImage ();

	#if ULTRA_INSPECTOR
		std::unique_ptr<melatonin::Inspector>	inspector;
	#endif

	gin::DownloadManager	downloader { 5 * 1000, 5 * 1000 };

	HVSCInstaller	hvscInstaller;
	AppUpdater		appUpdater { "https://ultrasid.com/api" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ultraSID )
};
//-----------------------------------------------------------------------------
