#pragma once

#include <JuceHeader.h>

#include "Data/Playlists.h"

//-----------------------------------------------------------------------------

// The playback queue: which playlist is playing, the position in it, and how
// to advance (shuffle, repeat, end-of-list wraparound). Pure model, the
// playlist views render this state, they don't own it. The underlying entry
// data always lives in the Playlists global; the queue resolves it on demand
// so playlist edits are picked up on the next advance.

class PlayQueue final
{
public:
	enum class Repeat : int8_t	// matches the transport button stages
	{
		off,
		one,
		all,
	};

	// The playing playlist, by name (empty = not playing from a playlist)
	void setPlaylist ( const std::string& name );
	[[ nodiscard ]] const std::string& getPlaylistName () const	{	return currentName;	}
	[[ nodiscard ]] bool isActive () const			{	return resolve () != nullptr;	}

	[[ nodiscard ]] int getSize () const;

	// What playing a queue item means
	struct Advance
	{
		bool	stopped = false;	// end of playlist reached with repeat off, reset the playing UI

		const Database::entry*	entry = nullptr;	// null = nothing to play
		int16_t	subtune = 0;
		int		playPosition = -1;
	};

	// Move by delta and resolve the item to play. Manual presses reshuffle and
	// wrap at either end of the list; automatic advancement only wraps on
	// repeat all, otherwise it stops at the end.
	[[ nodiscard ]] Advance advance ( const int delta, const Repeat repeat, const bool shuffleOn, const bool manual );

	// Queue state. position is the logical spot in the playlist,
	// playPosition the actual (possibly shuffled) row being played.
	// NOTE: a hand-picked row stores the *play* position into `position`, so
	// with shuffle on a pick restarts the shuffle walk from that row.
	int	position = -1;
	int	playPosition = -1;
	int	subtune = 0;

private:
	[[ nodiscard ]] playlist* resolve () const;
	void refresh () const;

	juce::SharedResourcePointer<Playlists>	playlists;

	std::string	currentName;

	// Entry data resolved from the underlying playlist
	mutable std::vector<const Database::entry*>	rowData;
	mutable std::vector<int16_t>				rowSubtunes;
};
//-----------------------------------------------------------------------------
