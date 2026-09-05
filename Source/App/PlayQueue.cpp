#include "PlayQueue.h"

//-----------------------------------------------------------------------------

void PlayQueue::setPlaylist ( const std::string& name )
{
	if ( auto pl = resolve () )
		pl->setQueuePositionLocation ( nullptr );

	currentName = name;

	if ( auto pl = resolve () )
		pl->setQueuePositionLocation ( &position );
}
//-----------------------------------------------------------------------------

playlist* PlayQueue::resolve () const
{
	if ( currentName.empty () )
		return nullptr;

	return playlists->getPlaylistItems ( currentName );
}
//-----------------------------------------------------------------------------

void PlayQueue::refresh () const
{
	rowData.clear ();
	rowSubtunes.clear ();

	if ( auto pl = resolve () )
		pl->createRowData ( rowData, rowSubtunes );
}
//-----------------------------------------------------------------------------

int PlayQueue::getSize () const
{
	refresh ();
	return int ( rowData.size () );
}
//-----------------------------------------------------------------------------

PlayQueue::Advance PlayQueue::advance ( const int delta, const Repeat repeat, const bool shuffleOn, const bool manual )
{
	refresh ();
	const auto	size = int ( rowData.size () );

	// With shuffle off the walk follows actual rows: continue from the row
	// that is audibly playing, not a logical spot left by a shuffled stretch
	if ( ! shuffleOn && playPosition >= 0 )
		position = playPosition;

	const auto	wraps = manual || repeat == Repeat::all;

	// Missing tunes (null entries) are skipped in the direction of travel; the
	// attempt bound stops a full wrap through a playlist with nothing playable
	const auto	step = delta < 0 ? -1 : 1;

	for ( auto move = delta, attempts = 0; attempts <= size; ++attempts, move = step )
	{
		position = std::clamp ( position + move, -1, size );

		// Off the start (previous on the first entry): restart from the far end
		if ( position < 0 )
		{
			if ( ! wraps || ! size )
				return {};

			if ( auto pl = resolve () )
				pl->createShuffle ();

			position = size - 1;
		}

		// Reached end of playlist?
		if ( position >= size )
		{
			if ( auto pl = resolve () )
				pl->createShuffle ();

			if ( ! wraps )
			{
				position = -1;
				return { .stopped = true };
			}

			position = 0;
		}

		playPosition = position;

		if ( shuffleOn )
		{
			auto	pl = resolve ();
			playPosition = pl ? pl->getShuffled ( position ) : -1;
		}

		if ( playPosition < 0 || playPosition >= size )
			return {};

		if ( rowData[ playPosition ] )
			return { false, rowData[ playPosition ], rowSubtunes[ playPosition ], playPosition };

		// Repeat-one of a tune that went missing mid-play: stop, don't wander
		if ( delta == 0 )
			return {};
	}

	return {};
}
//-----------------------------------------------------------------------------
