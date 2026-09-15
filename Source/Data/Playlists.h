#pragma once

#include <JuceHeader.h>

#include <array>

#include "Database/Database.h"

//-----------------------------------------------------------------------------

class playlist
{
public:
	playlist ( const juce::String& name );

	[[ nodiscard ]] juce::String getName () const { return name; }
	void setName ( const juce::String& newName ) { name = newName; }

	void load ();
	bool save ();

	// The commit step after an entry edit: reshuffle, persist, notify views
	void saveAndNotify ();
	void deleteFile ();
	void deleteCover ();
	[[ nodiscard ]] bool hasWriteAccess () const	{ 	return getFile ().hasWriteAccess (); }

	void rename ( const juce::String& newName );
	[[ nodiscard ]] bool hasCover () const { return coverExtension.isNotEmpty (); }

	// Cover park & restore for undo, the file on disk stays untouched
	struct Cover
	{
		juce::String	extension;
		juce::Image		image;
	};
	[[ nodiscard ]] Cover takeCover ();
	void restoreCover ( const Cover& cover );

	// Every index in this API is a VIEW row. Sorted, the view is a permutation
	// over the entries, which keep the file's custom order; unsorted, the view
	// row IS the entry index
	void setSort ( const db::SortKey key, const bool forwards );
	void resort ()	{	setSort ( sortKey, sortForwards );	}
	[[ nodiscard ]] db::SortKey getSortKey () const	{	return sortKey;	}
	[[ nodiscard ]] bool isSortedForwards () const	{	return sortForwards;	}
	[[ nodiscard ]] bool isSorted () const			{	return sortKey != db::SortKey::none;	}

	// View row <-> position in the custom order
	[[ nodiscard ]] int getEntryIndex ( const int row ) const	{	return isSorted () ? viewOrder[ size_t ( row ) ] : row;	}
	[[ nodiscard ]] int getViewIndex ( const int entryIndex ) const;

	void clear ();
	void removeItem ( const int index, const bool isFinal = false );
	void removeItems ( const juce::SparseSet<int>& rows );

	// entryIndex = custom-order slot while sorted (-1 = end), ignored unsorted
	void addItem ( const std::string& tune, const int index = -1, const int entryIndex = -1 );
	void addItems ( const juce::StringArray& tunes );

	[[ nodiscard ]] std::string& getEntry ( const int index )  { return entries[ size_t ( getEntryIndex ( index ) ) ]; }

	// Same tunes in the same order
	[[ nodiscard ]] bool hasEntries ( const juce::StringArray& tunes ) const;

	void createShuffle ();
	[[ nodiscard ]] int getShuffled ( const int position ) const;

	void moveItems ( const juce::SparseSet<int>& rows, int insertIndex );

	[[ nodiscard ]] juce::File getFile () const;
	[[ nodiscard ]] juce::File getCoverFile () const;
	void setCoverFile ( const juce::String& name );
	[[ nodiscard ]] juce::Image getCoverImage () const	{ return coverImage; }

	void createRowData ( std::vector<const Database::entry*>& rowData, std::vector<int16_t>& rowDataSubtunes );

	[[ nodiscard ]] int getNumItems () const { return static_cast<int> ( entries.size () ); }

	// All three hold view rows; edits and re-sorts move them
	void setRowPlayingLocation ( int* row )			{ rowPlaying = row; }
	void setQueuePositionLocation ( int* position, int* playPosition )
	{
		queuePosition = position;
		queuePlayPosition = playPosition;
	}

private:
	[[ nodiscard ]] std::array<int*, 3> playingRows () const	{	return { rowPlaying, queuePosition, queuePlayPosition };	}

	juce::String	name;
	juce::String	coverExtension;

	std::vector<std::string>	entries;
	std::vector<int>			shuffleOrder;

	// Entry index per view row, empty without a sort
	std::vector<int>			viewOrder;
	db::SortKey					sortKey = db::SortKey::none;
	bool						sortForwards = true;

	int*	rowPlaying = nullptr;
	int*	queuePosition = nullptr;
	int*	queuePlayPosition = nullptr;

	juce::Image		coverImage;
};
//-----------------------------------------------------------------------------

class Playlists final
{
public:
	Playlists () = default;

	void findPlaylists ();
	[[ nodiscard ]] juce::StringArray getPlaylistNames () const;

	// Returns the name actually used, the file's own #PLAYLIST: line wins over the filename
	juce::String addPlaylist ( const juce::String& filename );

	// Stores M3U text as a new playlist file; a same-named list with identical
	// entries is reused instead. name is "" when the content is no M3U
	struct Imported
	{
		juce::String	name;
		bool			created = false;
	};
	Imported importPlaylist ( juce::String name, const juce::String& content );

	// Delete park & restore for undo, take leaves the files on disk intact
	[[ nodiscard ]] std::shared_ptr<playlist> takePlaylist ( const juce::String& name );
	void restorePlaylist ( const std::shared_ptr<playlist>& items );

	// Returns the name actually used, bumped when taken ("B" -> "B 2")
	juce::String renamePlaylist ( const juce::String& oldName, const juce::String& newName );

	juce::String addToPlaylist ( juce::String name, const juce::StringArray& tunes );
	[[ nodiscard ]] playlist* getPlaylistItems ( const std::string& name ) const;

	void setPlaylistCover ( const juce::String& name, const juce::String& file ) const;

	// The shared cover-drop tail: the first usable image becomes the cover
	void applyCoverDrop ( const juce::String& name, const juce::StringArray& files ) const;

private:
	// Bumps the name until neither the map nor the disk claims it
	[[ nodiscard ]] juce::String unusedPlaylistName ( juce::String name ) const;

	std::unordered_map<std::string, std::shared_ptr<playlist>>	plMap;
};
//-----------------------------------------------------------------------------
