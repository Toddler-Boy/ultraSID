#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Helpers/MessageRouter.h"

//-----------------------------------------------------------------------------

// Typed messages for the global message bus.
//
// Each message is a struct: named fields, a wire verb, encode () to the
// string form and decode () back from the parsed parameter list. Senders use
// msg::LoadTune { file, 3, "playlist", 7 }.send () and get their arguments
// compile-checked; the wire format (space-delimited, quote-aware, see
// msg::parseActionMessage) stays a private detail of this header.
// Construction and sending are separate on purpose, decode () builds these
// structs too, and a message can be inspected before it is sent.
//
// Transport: juce::ActionBroadcaster, thread-safe from anywhere, one
// delivery per send, in order, on the message thread.

namespace msg
{

// The transport, all the send () members below funnel through here.
// Null broadcaster = teardown, the message is dropped
template<typename E>
void send ( const E& e )
{
	if ( auto* broadcaster = UI::ab.load () )
		broadcaster->sendActionMessage ( e.encode () );
}
//-----------------------------------------------------------------------------

// Messages with no payload

#define SIMPLE_MESSAGE(Name, wire)								\
	struct Name													\
	{															\
		static constexpr auto	verb = wire;					\
		[[ nodiscard ]] juce::String encode () const	{	return verb;	}	\
		void send () const				{	msg::send ( *this );	}	\
	}

SIMPLE_MESSAGE ( RestoreState,		"restoreState" );
SIMPLE_MESSAGE ( ShowSearch,		"showSearch" );
SIMPLE_MESSAGE ( ShowAbout,			"showAbout" );
SIMPLE_MESSAGE ( CloseAbout,		"closeAbout" );
SIMPLE_MESSAGE ( ShowShortcuts,		"showShortcuts" );
SIMPLE_MESSAGE ( CloseShortcuts,	"closeShortcuts" );
SIMPLE_MESSAGE ( VolumeChanged,		"volumeChanged" );
SIMPLE_MESSAGE ( TagsToggled,		"tagsToggled" );

// Export queue changed: the user flavor also floats a +N/-N badge off the
// main-menu pill, the plain one (worker progress) only updates the pill
SIMPLE_MESSAGE ( UpdateExportBadge,		"updateExportBadge" );
SIMPLE_MESSAGE ( UpdateExportBadgeUser,	"updateExportBadgeUser" );
SIMPLE_MESSAGE ( HvscCheck,			"hvscCheck" );

// Screenshot/artwork editing (developer mode)
SIMPLE_MESSAGE ( ToggleFirstLuma,	"toggleFirstLuma" );
SIMPLE_MESSAGE ( ToggleFirstLumaAll,"toggleFirstLumaAll" );
SIMPLE_MESSAGE ( ToggleThumbnail,	"toggleThumbnail" );
SIMPLE_MESSAGE ( DeleteImage,		"deleteImage" );
SIMPLE_MESSAGE ( RemoveBorderColor,	"removeBorderColor" );

// The keyboard verbs (Data/UI/shortcuts.csv binds keys to these wire names)
SIMPLE_MESSAGE ( TogglePlay,		"togglePlay" );
SIMPLE_MESSAGE ( LikePlaying,		"likePlaying" );
SIMPLE_MESSAGE ( ToggleShuffle,		"toggleShuffle" );
SIMPLE_MESSAGE ( CycleRepeat,		"cycleRepeat" );
SIMPLE_MESSAGE ( PreviousTrack,		"previousTrack" );
SIMPLE_MESSAGE ( NextTrack,			"nextTrack" );
SIMPLE_MESSAGE ( SeekBack,			"seekBack" );
SIMPLE_MESSAGE ( SeekForward,		"seekForward" );
SIMPLE_MESSAGE ( VolumeUp,			"volumeUp" );
SIMPLE_MESSAGE ( VolumeDown,		"volumeDown" );
SIMPLE_MESSAGE ( ToggleMute,		"toggleMute" );
SIMPLE_MESSAGE ( ToggleQuality,		"toggleQuality" );
SIMPLE_MESSAGE ( FocusSearch,		"focusSearch" );
SIMPLE_MESSAGE ( ShowLiked,			"showLiked" );
SIMPLE_MESSAGE ( JumpToPlaying,		"jumpToPlaying" );
SIMPLE_MESSAGE ( ShowPlaylists,		"showPlaylists" );
SIMPLE_MESSAGE ( ShowSettings,		"showSettings" );
SIMPLE_MESSAGE ( NewPlaylist,		"newPlaylist" );
SIMPLE_MESSAGE ( Undo,				"undo" );
SIMPLE_MESSAGE ( ToggleFullscreen,	"toggleFullscreen" );

#undef SIMPLE_MESSAGE
//-----------------------------------------------------------------------------

// Download commands ("download <sub> [action]")

struct DownloadHVSC
{
	enum class action : int8_t { update, full, cancel, cancelUpdate };

	action	what = action::update;

	static constexpr auto	verb = "download";
	static constexpr auto	sub = "HVSC";

	[[ nodiscard ]] juce::String encode () const
	{
		static constexpr const char*	names[] = { "update", "full", "cancel", "cancelUpdate" };
		return juce::String ( verb ) + " " + sub + " " + names[ int ( what ) ];
	}

	[[ nodiscard ]] static DownloadHVSC decode ( const juce::StringArray& p )
	{
		if ( p[ 1 ] == "update" )		return { action::update };
		if ( p[ 1 ] == "full" )			return { action::full };
		if ( p[ 1 ] == "cancel" )		return { action::cancel };
		if ( p[ 1 ] == "cancelUpdate" )	return { action::cancelUpdate };

		jassertfalse;	// Unknown download action
		return {};
	}

	void send () const	{	msg::send ( *this );	}
};
//-----------------------------------------------------------------------------

// Navigation

struct ShowPage
{
	juce::String	name;

	static constexpr auto	verb = "showPage";
	[[ nodiscard ]] juce::String encode () const						{	return juce::String ( verb ) + " " + name;	}
	[[ nodiscard ]] static ShowPage decode ( const juce::StringArray& p )	{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};

struct MainMenu
{
	juce::String	name;

	static constexpr auto	verb = "mainMenu";
	[[ nodiscard ]] juce::String encode () const						{	return juce::String ( verb ) + " " + name;	}
	[[ nodiscard ]] static MainMenu decode ( const juce::StringArray& p )	{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};

struct ShowPlaylist
{
	juce::String	name;	// empty = leave detail view

	static constexpr auto	verb = "showPlaylist";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + quoted ( name );	}
	[[ nodiscard ]] static ShowPlaylist decode ( const juce::StringArray& p )	{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};

struct GoToFolder
{
	juce::String	folder;

	static constexpr auto	verb = "goToFolder";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + quoted ( folder );	}
	[[ nodiscard ]] static GoToFolder decode ( const juce::StringArray& p )		{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};

struct SetCRTPage
{
	int	page = 0;

	static constexpr auto	verb = "setCRTpage";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + juce::String ( page );	}
	[[ nodiscard ]] static SetCRTPage decode ( const juce::StringArray& p )		{	return { p[ 0 ].getIntValue () };	}

	void send () const	{	msg::send ( *this );	}
};

struct SetLocation
{
	juce::String	name;	// "hvsc" or "user"

	static constexpr auto	verb = "setLocation";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + name;	}
	[[ nodiscard ]] static SetLocation decode ( const juce::StringArray& p )	{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};
//-----------------------------------------------------------------------------

// Playback / transport

struct LoadTune
{
	juce::String	file;
	int				subtune = 0;
	juce::String	src;			// "playlist", "search", ...
	int				playlistPos = -1;

	static constexpr auto	verb = "loadTune";

	[[ nodiscard ]] juce::String encode () const
	{
		return juce::String ( verb ) + " " + quoted ( file ) + " " + juce::String ( subtune )
			 + " " + src + " " + juce::String ( playlistPos );
	}

	[[ nodiscard ]] static LoadTune decode ( const juce::StringArray& p )
	{
		return { p[ 0 ], p[ 1 ].getIntValue (), p[ 2 ], p[ 3 ].getIntValue () };
	}

	void send () const	{	msg::send ( *this );	}
};

struct PlaySubtune
{
	int	subtune = 0;	// 1-based

	static constexpr auto	verb = "playSubtune";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + juce::String ( subtune );	}
	[[ nodiscard ]] static PlaySubtune decode ( const juce::StringArray& p )	{	return { p[ 0 ].getIntValue () };	}

	void send () const	{	msg::send ( *this );	}
};

struct Transport
{
	juce::String	action;	// "play", "prev", "next"

	static constexpr auto	verb = "transport";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + action;	}
	[[ nodiscard ]] static Transport decode ( const juce::StringArray& p )		{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};

struct PlayPlaylist
{
	juce::String	name;

	static constexpr auto	verb = "playPlaylist";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + quoted ( name );	}
	[[ nodiscard ]] static PlayPlaylist decode ( const juce::StringArray& p )	{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};
//-----------------------------------------------------------------------------

// Settings / state

struct SettingChanged
{
	juce::String	section;
	juce::String	key;	// optional, "fx" broadcasts the whole section

	static constexpr auto	verb = "settingChanged";

	[[ nodiscard ]] juce::String encode () const
	{
		return juce::String ( verb ) + " " + section + ( key.isEmpty () ? juce::String () : " " + key );
	}

	[[ nodiscard ]] static SettingChanged decode ( const juce::StringArray& p )	{	return { p[ 0 ], p[ 1 ] };	}

	[[ nodiscard ]] juce::String sectionKey () const	{	return section + "/" + key;	}

	void send () const	{	msg::send ( *this );	}
};

struct LikeChanged
{
	juce::String	file;
	int				subtune = 0;

	static constexpr auto	verb = "likeChanged";
	[[ nodiscard ]] juce::String encode () const						{	return juce::String ( verb ) + " " + quoted ( file ) + " " + juce::String ( subtune );	}
	[[ nodiscard ]] static LikeChanged decode ( const juce::StringArray& p )	{	return { p[ 0 ], p[ 1 ].getIntValue () };	}

	void send () const	{	msg::send ( *this );	}
};

struct AssignBorderColor
{
	int	index = 0;

	static constexpr auto	verb = "assignBorderColor";
	[[ nodiscard ]] juce::String encode () const								{	return juce::String ( verb ) + " " + juce::String ( index );	}
	[[ nodiscard ]] static AssignBorderColor decode ( const juce::StringArray& p )	{	return { p[ 0 ].getIntValue () };	}

	void send () const	{	msg::send ( *this );	}
};
//-----------------------------------------------------------------------------

// Export / downloads

struct ExportTune
{
	juce::StringArray	tunes;	// "file,subtune" each

	static constexpr auto	verb = "exportTune";

	// One parameter per tune: quoted () already makes a field safe for any
	// filename, so the list needs no separator of its own
	[[ nodiscard ]] juce::String encode () const
	{
		auto	out = juce::String ( verb );

		for ( const auto& tune : tunes )
			out += " " + quoted ( tune );

		return out;
	}

	[[ nodiscard ]] static ExportTune decode ( const juce::StringArray& p )		{	return { p };	}

	void send () const	{	msg::send ( *this );	}
};

// An export entry reached a non-stage status (finished, canceled, paused,
// requeued), sent by the worker threads so the UI repaints just that row
struct ExportEntryStatusUpdate
{
	int	index = 0;

	static constexpr auto	verb = "exportEntryStatusUpdate";
	[[ nodiscard ]] juce::String encode () const								{	return juce::String ( verb ) + " " + juce::String ( index );	}
	[[ nodiscard ]] static ExportEntryStatusUpdate decode ( const juce::StringArray& p )	{	return { p[ 0 ].getIntValue () };	}

	void send () const	{	msg::send ( *this );	}
};

struct DownloadScreenshot
{
	juce::String	url;

	static constexpr auto	verb = "downloadScreenshot";
	[[ nodiscard ]] juce::String encode () const								{	return juce::String ( verb ) + " " + quoted ( url );	}
	[[ nodiscard ]] static DownloadScreenshot decode ( const juce::StringArray& p )	{	return { p[ 0 ] };	}

	void send () const	{	msg::send ( *this );	}
};

struct DownloadCover
{
	juce::String	playlist;
	juce::String	url;

	static constexpr auto	verb = "downloadCover";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + quoted ( playlist ) + " " + quoted ( url );	}
	[[ nodiscard ]] static DownloadCover decode ( const juce::StringArray& p )	{	return { p[ 0 ], p[ 1 ] };	}

	void send () const	{	msg::send ( *this );	}
};

struct AddScreenshots
{
	juce::StringArray	files;

	static constexpr auto	verb = "addScreenshots";

	[[ nodiscard ]] juce::String encode () const
	{
		juce::String	out ( verb );
		for ( const auto& f : files )
			out += " " + quoted ( f );
		return out;
	}

	[[ nodiscard ]] static AddScreenshots decode ( const juce::StringArray& p )	{	return { p };	}

	void send () const	{	msg::send ( *this );	}
};
//-----------------------------------------------------------------------------

// Playlist CRUD ("playlist <action> ...")

#define PLAYLIST_MESSAGE(Name, wire)																			\
	struct Name																									\
	{																											\
		juce::String	name;																					\
																												\
		static constexpr auto	verb = "playlist";																\
		static constexpr auto	sub = wire;																		\
		[[ nodiscard ]] juce::String encode () const					{	return juce::String ( verb ) + " " + sub + " " + quoted ( name );	}	\
		[[ nodiscard ]] static Name decode ( const juce::StringArray& p )	{	return { p[ 1 ] };	}							\
		void send () const	{	msg::send ( *this );	}													\
	}

PLAYLIST_MESSAGE ( PlaylistNew,			"new" );
PLAYLIST_MESSAGE ( PlaylistAddTo,		"addTo" );
PLAYLIST_MESSAGE ( PlaylistUpdate,		"update" );
PLAYLIST_MESSAGE ( PlaylistDeleteList,	"deleteList" );
PLAYLIST_MESSAGE ( PlaylistUpdateInfo,	"updateInfo" );

#undef PLAYLIST_MESSAGE

// Floating +N/-N pill over a grid tile (screen coords)
struct BadgeSpawn
{
	int	x = 0;
	int	y = 0;
	int	delta = 0;

	static constexpr auto	verb = "badgeSpawn";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + juce::String ( x ) + " " + juce::String ( y ) + " " + juce::String ( delta );	}
	[[ nodiscard ]] static BadgeSpawn decode ( const juce::StringArray& p )	{	return { p[ 0 ].getIntValue (), p[ 1 ].getIntValue (), p[ 2 ].getIntValue () };	}

	void send () const	{	msg::send ( *this );	}
};

struct PlaylistRenamed
{
	juce::String	oldName;
	juce::String	newName;

	static constexpr auto	verb = "playlist";
	static constexpr auto	sub = "renamed";
	[[ nodiscard ]] juce::String encode () const							{	return juce::String ( verb ) + " " + sub + " " + quoted ( oldName ) + " " + quoted ( newName );	}
	[[ nodiscard ]] static PlaylistRenamed decode ( const juce::StringArray& p )	{	return { p[ 1 ], p[ 2 ] };	}

	void send () const	{	msg::send ( *this );	}
};

}	// namespace msg
//-----------------------------------------------------------------------------
