#include <array>
#include <numeric>
#include <unordered_set>

#include "Playlists.h"

#include "ultra-shared/Helpers/FileUtils.h"
#include "ultra-shared/Helpers/TextUtils.h"
#include "ultra-shared/Resources/Strings.h"

#include "Config/FilePaths.h"
#include "Database/Database.h"
#include "Database/TuneInfo.h"
#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

static juce::File playlistFile ( const juce::String& name );

// Strips a default subtune. Only an all-digit tail counts, a comma may belong to the file name
static std::string bareDefault ( std::string entry )
{
	const auto	comma = entry.find_last_of ( ',' );
	if ( comma == std::string::npos || comma + 1 == entry.size () )
		return entry;

	const auto	number = std::string_view ( entry ).substr ( comma + 1 );
	if ( ! std::ranges::all_of ( number, [] ( const char c ) { return c >= '0' && c <= '9'; } ) )
		return entry;

	const auto	subTune = std::atoi ( number.data () );
	const auto	dbEnt = db::findDatabaseEntry ( entry.substr ( 0, comma ) );

	if ( subTune == 0 || ( dbEnt && dbEnt->startTune == subTune ) )
		entry.resize ( comma );

	return entry;
}
//-----------------------------------------------------------------------------

// Equal for every spelling of one tune
static std::string tuneId ( const std::string& entry )
{
	const auto [ tuneName, subTune ] = SID::parseTuneName ( entry );

	return tuneName + "," + std::to_string ( db::realSubtune ( db::findDatabaseEntry ( tuneName ), subTune ) );
}
//-----------------------------------------------------------------------------

// "Tunes" -> "Tunes 2" -> "Tunes 3"
static juce::String bumpTrailingNumber ( juce::String name )
{
	if ( juce::CharacterFunctions::isDigit ( name.getLastCharacter () ) )
	{
		const auto	num = name.getTrailingIntValue ();
		name = name.dropLastCharacters ( juce::String ( num ).length () );
		return name + juce::String ( num + 1 );
	}

	return name + " 2";
}
//-----------------------------------------------------------------------------

// The tune lines and #PLAYLIST: name of M3U text, valid only with the header
struct M3U
{
	bool				valid = false;
	juce::String		name;
	juce::StringArray	entries;
};

static M3U parseM3U ( const juce::String& text )
{
	auto	lines = juce::StringArray::fromLines ( text );

	lines.removeEmptyStrings ();
	lines.trim ();

	if ( lines.isEmpty () || lines.getReference ( 0 ) != "#EXTM3U" )
		return {};

	auto	m3u = M3U { true };

	for ( const auto& line : lines )
	{
		if ( line.startsWithChar ( '#' ) )
		{
			if ( line.startsWith ( "#PLAYLIST:" ) )
				m3u.name = line.fromFirstOccurrenceOf ( ":", false, false );
		}
		else
			m3u.entries.add ( line );
	}

	return m3u;
}
//-----------------------------------------------------------------------------

void Playlists::findPlaylists ()
{
	plMap.clear ();

	auto	path = filepaths::getPlaylistsPath ();
	if ( path == juce::File () )
		return;

	// Load playlists into memory
	auto	listsAsFileArray = path.findChildFiles ( juce::File::findFiles | juce::File::ignoreHiddenFiles, false, "*.m3u" );
	for ( const auto& file : listsAsFileArray )
		addPlaylist ( file.getFileNameWithoutExtension () );
}
//-----------------------------------------------------------------------------

juce::StringArray Playlists::getPlaylistNames () const
{
	auto	names = juce::StringArray {};

	for ( const auto& entry : plMap )
		names.add ( entry.first );

	names.sortNatural ();

	return names;
}
//-----------------------------------------------------------------------------

juce::String Playlists::addPlaylist ( const juce::String& filename )
{
	auto	plItem = std::make_shared<playlist> ( filename );
	auto	name = plItem->getName ();

	plMap[ name.toStdString () ] = std::move ( plItem );

	return name;
}
//-----------------------------------------------------------------------------

Playlists::Imported Playlists::importPlaylist ( juce::String name, const juce::String& content )
{
	const auto	m3u = parseM3U ( content );
	if ( ! m3u.valid )
		return {};

	// The file's own name wins, as it does on load
	if ( m3u.name.isNotEmpty () )
		name = m3u.name;

	// "" would resolve to a hidden ".m3u"
	if ( name.trim ().isEmpty () )
	{
		const juce::SharedResourcePointer<Strings>	strings;
		name = strings->get ( "playlist/default_name" );
	}

	if ( const auto items = getPlaylistItems ( name.toStdString () ); items && items->hasEntries ( m3u.entries ) )
		return { name, false };

	const auto	file = playlistFile ( unusedPlaylistName ( name ) );
	file.getParentDirectory ().createDirectory ();

	if ( ! fileutils::replaceFile ( file, content ) )
		return {};

	return { addPlaylist ( file.getFileNameWithoutExtension () ), true };
}
//-----------------------------------------------------------------------------

juce::String Playlists::unusedPlaylistName ( juce::String name ) const
{
	// The disk counts too, a parked delete still holds its file
	const auto	playlistNames = getPlaylistNames ();

	while ( playlistNames.contains ( name ) || playlistFile ( name ).existsAsFile () )
		name = bumpTrailingNumber ( name );

	return name;
}
//-----------------------------------------------------------------------------

std::shared_ptr<playlist> Playlists::takePlaylist ( const juce::String& name )
{
	auto	it = plMap.find ( name.toStdString () );
	if ( it == plMap.end () )
		return nullptr;

	auto	items = std::move ( it->second );
	plMap.erase ( it );

	return items;
}
//-----------------------------------------------------------------------------

void Playlists::restorePlaylist ( const std::shared_ptr<playlist>& items )
{
	if ( items )
		plMap[ items->getName ().toStdString () ] = items;
}
//-----------------------------------------------------------------------------

juce::String Playlists::renamePlaylist ( const juce::String& oldName, const juce::String& newName )
{
	auto	items = getPlaylistItems ( oldName.toStdString () );
	jassert ( items );

	// "" is the sentinel for "no playlist visible", and would resolve to a hidden ".m3u"
	if ( newName.trim ().isEmpty () )
		return oldName;

	items->rename ( newName );

	// rename () may have bumped the name to keep it unique
	const auto	finalName = items->getName ();

	// Move item to new key in map
	if ( auto node = plMap.extract ( oldName.toStdString () ); ! node.empty () )
	{
		node.key () = finalName.toStdString ();
		plMap.insert ( std::move ( node ) );
	}

	return finalName;
}
//-----------------------------------------------------------------------------

juce::String Playlists::addToPlaylist ( juce::String name, const juce::StringArray& tunes )
{
	// Create new playlist
	if ( name.isEmpty () )
	{
		const juce::SharedResourcePointer<Strings>	strings;
		name = unusedPlaylistName ( strings->get ( "playlist/default_name" ) );

		addPlaylist ( name );
	}

	auto	items = getPlaylistItems ( name.toStdString () );
	if ( ! items )
		return {};

	// Add list of tunes
	for ( const auto& tune : tunes )
		items->addItem ( tune.toStdString () );

	items->createShuffle ();
	items->save ();

	return name;
}
//-----------------------------------------------------------------------------

playlist* Playlists::getPlaylistItems ( const std::string& name ) const
{
	if ( auto it = plMap.find ( name ); it != plMap.end () )
		return it->second.get ();

	return nullptr;
}
//-----------------------------------------------------------------------------

void Playlists::setPlaylistCover ( const juce::String& name, const juce::String& file ) const
{
	if ( auto items = getPlaylistItems ( name.toStdString () ); items )
		items->setCoverFile ( file );
}
//-----------------------------------------------------------------------------

void Playlists::applyCoverDrop ( const juce::String& name, const juce::StringArray& files ) const
{
	const auto	covers = textutils::getFilteredStrings ( files, { ".png", ".jpg" } );
	if ( covers.isEmpty () )
		return;

	setPlaylistCover ( name, covers[ 0 ] );
	msg::PlaylistUpdateInfo { name }.send ();
}
//-----------------------------------------------------------------------------

//
// playlist
//

static juce::File playlistFile ( const juce::String& name )
{
	return filepaths::getPlaylistsPath ().getChildFile ( juce::File::createLegalFileName ( name + ".m3u" ) );
}
//-----------------------------------------------------------------------------

// Bumps the name until no other playlist file claims it
static juce::String uniquePlaylistName ( juce::String name, const juce::File& ownFile )
{
	for ( ;; )
	{
		const auto	file = playlistFile ( name );

		if ( file == ownFile || ! file.existsAsFile () )
			return name;

		name = bumpTrailingNumber ( name );
	}
}
//-----------------------------------------------------------------------------

playlist::playlist ( const juce::String& _name )
	: name ( _name )
{
	load ();
}
//-----------------------------------------------------------------------------

void playlist::load ()
{
	coverExtension = "";
	coverImage = {};

	auto	file = getFile ();
	if ( ! file.existsAsFile () )
		return;

	const auto	m3u = parseM3U ( file.loadFileAsString () );
	if ( ! m3u.valid )
		return;

	//
	// Check for playlist image
	//
	if ( file.withFileExtension ( ".png" ).existsAsFile () )		coverExtension = ".png";
	else if ( file.withFileExtension ( ".jpg" ).existsAsFile () )	coverExtension = ".jpg";

	coverImage = juce::ImageFileFormat::loadFrom ( getCoverFile () );

	sortKey = db::SortKey::none;
	clear ();
	addItems ( m3u.entries );
	createShuffle ();

	// The file's internal name is authoritative, adopt it and move the file
	// (rename () saves the loaded entries under the new name)
	if ( m3u.name.isNotEmpty () && m3u.name != getName () )
		rename ( m3u.name );
}
//-----------------------------------------------------------------------------

bool playlist::hasEntries ( const juce::StringArray& tunes ) const
{
	if ( tunes.size () != getNumItems () )
		return false;

	for ( auto i = 0; i < tunes.size (); ++i )
		if ( entries[ i ] != bareDefault ( tunes[ i ].toStdString () ) )
			return false;

	return true;
}
//-----------------------------------------------------------------------------

bool playlist::save ()
{
	std::string	list;

	list += "#EXTM3U\n";
	list += "#PLAYLIST:" + getName ().toStdString () + "\n\n";

	for ( const auto& ret : entries )
		list += ret + "\n";

	auto	file = getFile ();
	file.getParentDirectory ().createDirectory ();

	return fileutils::replaceFile ( file, list.c_str (), list.size () );
}
//-----------------------------------------------------------------------------

void playlist::saveAndNotify ()
{
	createShuffle ();
	save ();

	msg::PlaylistUpdate { getName () }.send ();
}
//-----------------------------------------------------------------------------

void playlist::deleteFile ()
{
	deleteCover ();

	getFile ().deleteFile ();
}
//-----------------------------------------------------------------------------

void playlist::deleteCover ()
{
	getCoverFile ().deleteFile ();
	coverExtension = "";

	coverImage = {};
}
//-----------------------------------------------------------------------------

playlist::Cover playlist::takeCover ()
{
	auto	cover = Cover { coverExtension, coverImage };

	coverExtension = "";
	coverImage = {};

	return cover;
}
//-----------------------------------------------------------------------------

void playlist::restoreCover ( const Cover& cover )
{
	coverExtension = cover.extension;
	coverImage = cover.image;
}
//-----------------------------------------------------------------------------

void playlist::rename ( const juce::String& newName )
{
	// Backup old file object
	auto	file = getFile ();
	auto	cover = getCoverFile ();

	// Save under new name, bumped if another playlist already claims it
	setName ( uniquePlaylistName ( newName, file ) );

	if ( getFile () != file )
	{
		// The old file is the only copy until the new one is on disk
		if ( ! save () )
			return;

		// Delete old file
		file.deleteFile ();

		// Rename cover
		cover.moveFileTo ( getCoverFile () );
	}
}
//-----------------------------------------------------------------------------

void playlist::clear ()
{
	entries.clear ();
	viewOrder.clear ();
}
//-----------------------------------------------------------------------------

void playlist::setSort ( const db::SortKey key, const bool forwards )
{
	std::array<int, 3>	playingEntries { -1, -1, -1 };

	for ( auto i = 0u; auto* row : playingRows () )
	{
		if ( row && juce::isPositiveAndBelow ( *row, getNumItems () ) )
			playingEntries[ i ] = getEntryIndex ( *row );
		++i;
	}

	sortKey = key;
	sortForwards = forwards;

	computeViewOrder ();

	for ( auto i = 0u; auto* row : playingRows () )
	{
		if ( row && playingEntries[ i ] >= 0 )
			*row = getViewIndex ( playingEntries[ i ] );
		++i;
	}
}
//-----------------------------------------------------------------------------

void playlist::computeViewOrder ()
{
	viewOrder.clear ();

	if ( ! isSorted () )
		return;

	const auto	size = entries.size ();

	std::vector<db::SortItem>	items ( size );
	for ( auto i = 0u; i < size; ++i )
	{
		const auto [ tuneName, subTune ] = SID::parseTuneName ( entries[ i ] );
		items[ i ] = db::sortItem ( sortKey, db::findDatabaseEntry ( tuneName ), subTune );
	}

	viewOrder.resize ( size );
	std::iota ( viewOrder.begin (), viewOrder.end (), 0 );

	// Ties keep the custom order
	std::ranges::stable_sort ( viewOrder, [ & ] ( const int a, const int b )
	{
		return db::entryLess ( sortKey, sortForwards, items[ size_t ( a ) ], items[ size_t ( b ) ] );
	} );
}
//-----------------------------------------------------------------------------

void playlist::keepOrder ()
{
	if ( ! isSorted () )
		return;

	std::vector<std::string>	ordered;
	ordered.reserve ( entries.size () );

	for ( const auto e : viewOrder )
		ordered.push_back ( std::move ( entries[ size_t ( e ) ] ) );

	entries = std::move ( ordered );

	viewOrder.clear ();
	sortKey = db::SortKey::none;
}
//-----------------------------------------------------------------------------

void playlist::restoreOrder ( std::vector<std::string> oldEntries, const db::SortKey key, const bool forwards )
{
	const auto	tunes = playingTunes ();

	entries = std::move ( oldEntries );

	sortKey = key;
	sortForwards = forwards;

	computeViewOrder ();
	followPlayingTunes ( tunes );
}
//-----------------------------------------------------------------------------

void playlist::shuffleEntries ()
{
	const auto	tunes = playingTunes ();

	auto	rd = std::random_device {};
	auto	rng = std::default_random_engine { rd () };

	std::ranges::shuffle ( entries, rng );

	viewOrder.clear ();
	sortKey = db::SortKey::none;

	followPlayingTunes ( tunes );
}
//-----------------------------------------------------------------------------

bool playlist::hasDuplicates () const
{
	std::unordered_set<std::string>	seen;

	for ( const auto& entry : entries )
		if ( ! seen.insert ( tuneId ( entry ) ).second )
			return true;

	return false;
}
//-----------------------------------------------------------------------------

int playlist::removeDuplicates ()
{
	const auto	tunes = playingTunes ();
	const auto	oldSize = getNumItems ();

	std::unordered_set<std::string>	seen;
	std::vector<std::string>		unique;

	for ( auto& entry : entries )
		if ( seen.insert ( tuneId ( entry ) ).second )
			unique.push_back ( std::move ( entry ) );

	entries = std::move ( unique );

	computeViewOrder ();
	followPlayingTunes ( tunes );

	return oldSize - getNumItems ();
}
//-----------------------------------------------------------------------------

std::array<std::string, 3> playlist::playingTunes ()
{
	std::array<std::string, 3>	tunes;

	for ( auto i = 0u; auto* row : playingRows () )
	{
		if ( row && juce::isPositiveAndBelow ( *row, getNumItems () ) )
			tunes[ i ] = tuneId ( getEntry ( *row ) );
		++i;
	}

	return tunes;
}
//-----------------------------------------------------------------------------

void playlist::followPlayingTunes ( const std::array<std::string, 3>& tunes )
{
	for ( auto i = 0u; auto* row : playingRows () )
	{
		const auto&	tune = tunes[ i++ ];

		if ( tune.empty () )
			continue;

		if ( juce::isPositiveAndBelow ( *row, getNumItems () ) && tuneId ( getEntry ( *row ) ) == tune )
			continue;

		*row = -1;

		for ( auto r = 0; r < getNumItems (); ++r )
		{
			if ( tuneId ( getEntry ( r ) ) == tune )
			{
				*row = r;
				break;
			}
		}
	}
}
//-----------------------------------------------------------------------------

int playlist::getViewIndex ( const int entryIndex ) const
{
	if ( ! isSorted () )
		return entryIndex;

	const auto	it = std::ranges::find ( viewOrder, entryIndex );

	return it == viewOrder.end () ? -1 : int ( it - viewOrder.begin () );
}
//-----------------------------------------------------------------------------

void playlist::removeItem ( const int index, const bool isFinal )
{
	const auto	entryIndex = getEntryIndex ( index );

	entries.erase ( entries.begin () + entryIndex );

	if ( isSorted () )
	{
		viewOrder.erase ( viewOrder.begin () + index );

		for ( auto& e : viewOrder )
			if ( e > entryIndex )
				--e;
	}

	// The playing row on same location or below removal index
	for ( auto* row : playingRows () )
	{
		if ( ! row )
			continue;

		if ( isFinal && *row == index )
			*row = -1;
		else if ( *row >= index )
			--*row;
	}
}
//-----------------------------------------------------------------------------

void playlist::removeItems ( const juce::SparseSet<int>& rows )
{
	// Back to front, removals don't shift the rows still to visit
	for ( auto i = rows.size () - 1; i >= 0; --i )
		removeItem ( rows[ i ] );
}
//-----------------------------------------------------------------------------

void playlist::addItem ( const std::string& tune, const int index, const int entryIndex )
{
	const auto	size = getNumItems ();

	auto	slot = isSorted () ? entryIndex : index;
	if ( slot < 0 || slot >= size )
		slot = size;

	entries.insert ( entries.begin () + slot, bareDefault ( tune ) );

	if ( isSorted () )
	{
		for ( auto& e : viewOrder )
			if ( e >= slot )
				++e;

		const auto	row = index < 0 || index >= size ? size : index;
		viewOrder.insert ( viewOrder.begin () + row, slot );
	}

	// Playing row on same location or below insertion row
	if ( index >= 0 && index < size )
		for ( auto* row : playingRows () )
			if ( row && *row >= index )
				++*row;
}
//-----------------------------------------------------------------------------

void playlist::addItems ( const juce::StringArray& tunes )
{
	for ( const auto& str : tunes )
		addItem ( str.toStdString () );
}
//-----------------------------------------------------------------------------

void playlist::createShuffle ()
{
	const auto	size = int ( entries.size () );

	// Fill vector with ascending numbers
	shuffleOrder.clear ();
	shuffleOrder.reserve ( size );

	for ( auto idx = 0; idx < size; ++idx )
		shuffleOrder.emplace_back ( idx );

	// Shuffle numbers into random order
	auto	rd = std::random_device {};
	auto	rng = std::default_random_engine { rd () };

	std::ranges::shuffle ( shuffleOrder, rng );
}
//-----------------------------------------------------------------------------

int playlist::getShuffled ( const int position ) const
{
	if ( position < 0 || position >= int ( shuffleOrder.size () ) )
		return -1;

	return shuffleOrder[ position ];
}
//-----------------------------------------------------------------------------

juce::File playlist::getFile () const
{
	return playlistFile ( getName () );
}
//-----------------------------------------------------------------------------

juce::File playlist::getCoverFile () const
{
	if ( coverExtension.isEmpty () )
		return {};

	return getFile ().withFileExtension ( coverExtension );
}
//-----------------------------------------------------------------------------

void playlist::setCoverFile ( const juce::String& name )
{
	deleteCover ();

	auto	srcFile = juce::File ( name );
	coverExtension = srcFile.getFileExtension ();

	auto	target = getFile ().withFileExtension ( coverExtension );

	srcFile.copyFileTo ( target );

	coverImage = juce::ImageFileFormat::loadFrom ( target );
}
//-----------------------------------------------------------------------------

void playlist::createRowData ( std::vector<const Database::entry*>& rowData, std::vector<int16_t>& rowDataSubtunes )
{
	const auto	size = entries.size ();

	rowData.resize ( size );
	rowDataSubtunes.resize ( size );

	for ( auto i = 0u; i < size; ++i )
	{
		const auto [ tuneName, subTune ] = SID::parseTuneName ( entries[ size_t ( getEntryIndex ( int ( i ) ) ) ] );

		rowData[ i ] = db::findDatabaseEntry ( tuneName );
		rowDataSubtunes[ i ] = subTune;
	}
}
//-----------------------------------------------------------------------------

void playlist::moveItems ( const juce::SparseSet<int>& rows, int insertIndex )
{
	// Sorted, only the view permutation moves; the entries keep their order
	std::vector<int>	order;

	if ( isSorted () )
	{
		order = viewOrder;
	}
	else
	{
		order.resize ( entries.size () );
		std::iota ( order.begin (), order.end (), 0 );
	}

	std::array<int, 3>	playingEntries { -1, -1, -1 };

	for ( auto i = 0u; auto* row : playingRows () )
	{
		if ( row && juce::isPositiveAndBelow ( *row, getNumItems () ) )
			playingEntries[ i ] = order[ size_t ( *row ) ];
		++i;
	}

	std::vector<int>	moved;
	moved.reserve ( size_t ( rows.size () ) );

	for ( auto i = 0; i < rows.size (); ++i )
		moved.push_back ( order[ size_t ( rows[ i ] ) ] );

	// Back to front, removals don't shift the rows still to visit
	for ( auto rowIndex = rows.size () - 1; rowIndex >= 0; --rowIndex )
	{
		const auto	row = rows[ rowIndex ];

		if ( row < insertIndex )
			--insertIndex;

		order.erase ( order.begin () + row );
	}

	// Without a row to insert at, the block moves to the end
	if ( insertIndex < 0 || insertIndex > int ( order.size () ) )
		insertIndex = int ( order.size () );

	order.insert ( order.begin () + insertIndex, moved.begin (), moved.end () );

	if ( isSorted () )
	{
		viewOrder = std::move ( order );
	}
	else
	{
		std::vector<std::string>	reordered;
		reordered.reserve ( entries.size () );

		for ( const auto e : order )
			reordered.push_back ( std::move ( entries[ size_t ( e ) ] ) );

		entries = std::move ( reordered );
	}

	for ( auto i = 0u; auto* row : playingRows () )
	{
		if ( row && playingEntries[ i ] >= 0 )
			*row = getViewIndex ( playingEntries[ i ] );
		++i;
	}
}
//-------------------------------------------------------------------------------------------------
