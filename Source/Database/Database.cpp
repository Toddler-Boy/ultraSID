#include <array>

#include "Database.h"

#include "std_lime/lime_string_utils.h"

#include "Config/FilePaths.h"
#include "Database/TuneNames.h"

//-----------------------------------------------------------------------------

// Key folding, identical to lime::str::toLower; text fields use sortingLut
static constexpr auto asciiLowerLut = [] {
	std::array<uint8_t, 256>	lut {};

	for ( auto i = 0; i < 256; ++i )
		lut[ i ] = uint8_t ( i >= 'A' && i <= 'Z' ? i + 0x20 : i );

	return lut;
} ();
//-----------------------------------------------------------------------------

static void foldCorpus ( char* const data, const size_t len, const uint8_t* const lut )
{
	for ( size_t i = 0; i < len; ++i )
		data[ i ] = char ( lut[ uint8_t ( data[ i ] ) ] );
}
//-----------------------------------------------------------------------------

int Database::load ( const juce::MemoryBlock& mb )
{
	db = {};
	stringArena = {};
	searchArena = {};
	hvscVersion = 0;

	if ( mb.getSize () < usid::headerSize )
		return {};

	auto	src = (uint8_t*)mb.getData ();

	// Header
	if ( std::memcmp ( src, usid::magic, sizeof ( usid::magic ) ) )
		return {};
	src += 4;

	// The version counts only once the file checks out, a failed load reports 0
	const auto	version = int ( *src++ );
	if ( version < 84 )		// If the database is not at least for HVSC version 84, something is broken
		return {};

	auto get_u32 = [ &src ]		{	auto ret = *( (uint32_t*)src );	src += 4; return ret;	};

	// Get payload length
	auto	payloadLength = get_u32 ();

	// A corrupt or truncated file
	if ( mb.getSize () - usid::headerSize != payloadLength )
		return {};

	// Get number of entries
	auto	numEntries = get_u32 ();

	// Less entries than HVSC 85 contains, or more than it can realistically grow to
	if ( numEntries < 60'300 || numEntries > 70'000 )
		return {};

	hvscVersion = version;

	// The db is produced by sid_scanner and the payload size is verified
	// above, the parser deliberately trusts every length field

	// Pre-allocate memory for all tune entries
	db.reserve ( numEntries );

	// Pre-scan for exact arena sizes and the subtune-overflow total
	size_t	stringBytes = 0, corpusBytes = 0;
	{
		allSubtuneProperties = {};

		auto	totalSubtunesWithMoreThanMax = 0;

		auto	tempSrc = src;
		auto	tempNumEntries = numEntries;

		while ( tempNumEntries-- )
		{
			const auto	fileLen = size_t ( *tempSrc++ );
			tempSrc += fileLen;

			size_t	textLen = 0;
			for ( auto i = 0; i < 3; ++i )		// name, author, release
			{
				const auto	len = size_t ( *tempSrc++ );
				tempSrc += len;
				textLen += len;
			}

			tempSrc += 2;			// Skip flags
			tempSrc += 2;			// Skip startTune
			const auto	numTunes = *( (uint16_t*)tempSrc );
			tempSrc += 2;			// Skip numTunes
			tempSrc += numTunes * usid::wordsPerSubtune * sizeof ( int16_t );	// Skip tune properties

			if ( numTunes > maxTunesArray )
				totalSubtunesWithMoreThanMax += ( numTunes - 1 ) * usid::wordsPerSubtune;	// With the pointer in use, the array tail stores the first pair

			// "$HVSC$<path>.sid" + the three text fields; the corpus line
			// additionally carries its three field separators
			stringBytes += 6 + fileLen + 4 + textLen;
			corpusBytes += 6 + fileLen + 4 + textLen + 3;
		}

		// One arena for the subtunes of every tune with more than maxTunesArray of them:
		// 97.3% have fewer than 6, so per-tune arrays that size would waste memory,
		// and the arena frees in one go when the database is unloaded
		allSubtuneProperties.resize ( totalSubtunesWithMoreThanMax );
	}

	stringArena.resize ( stringBytes );
	searchArena.resize ( corpusBytes );

	auto get_u16 = [ &src ]		{	auto ret = *( (uint16_t*)src );	src += 2; return ret;	};
	auto get_cstring = [ &src ] {	const auto len = *src++; auto ret = (const char*)src; src += len; return std::pair<const char* const, const int>{ ret, len };	};

	auto put = [] ( char*& dst, const char* const s, const size_t n )	{	std::memcpy ( dst, s, n ); dst += n;	};

	auto	sp = stringArena.data ();
	auto	cp = searchArena.data ();

	auto	dstToUse = allSubtuneProperties.data ();
	while ( numEntries-- )
	{
		const auto	path = get_cstring ();
		const auto	name = get_cstring ();
		const auto	author = get_cstring ();
		const auto	release = get_cstring ();

		const auto	flags = get_u16 ();

		const auto	startTune = get_u16 ();
		const auto	numTunes = get_u16 ();

		const auto	tunePropPtr = reinterpret_cast<int16_t*> ( src );
		src += numTunes * usid::wordsPerSubtune * sizeof ( *tunePropPtr );

		// Originals: key, name, author, release, packed back to back
		const auto	keyStart = sp;
		std::memcpy ( sp, filepaths::hvscMarker.data (), filepaths::hvscMarker.size () );	sp += filepaths::hvscMarker.size ();
		std::memcpy ( sp, path.first, path.second );		sp += path.second;
		std::memcpy ( sp, ".sid", 4 );						sp += 4;
		const auto	keyLen = size_t ( sp - keyStart );

		const auto	nameStart = sp;		put ( sp, name.first, name.second );
		const auto	authorStart = sp;	put ( sp, author.first, author.second );
		const auto	releaseStart = sp;	put ( sp, release.first, release.second );

		// The folded line: NUL-separated fields
		const auto	lineStart = cp;
		put ( cp, keyStart, keyLen );				*cp++ = 0;
		const auto	textStart = cp;
		put ( cp, name.first, name.second );		*cp++ = 0;
		put ( cp, author.first, author.second );	*cp++ = 0;
		put ( cp, release.first, release.second );

		foldCorpus ( lineStart, keyLen, asciiLowerLut.data () );
		foldCorpus ( textStart, size_t ( cp - textStart ), sortingLut );

		const auto	nameLen = size_t ( name.second ), authorLen = size_t ( author.second ), releaseLen = size_t ( release.second );

		auto&	ent = db.emplace ( std::string_view ( keyStart, keyLen ), entry {

			.file = { keyStart, keyLen },
			.name = { nameStart, nameLen },
			.author = { authorStart, authorLen },
			.release = { releaseStart, releaseLen },

			.search = { lineStart, size_t ( cp - lineStart ) },
			.lowerFile = { lineStart, keyLen },
			.lowerName = { textStart, nameLen },
			.lowerRelease = { textStart + nameLen + 1 + authorLen + 1, releaseLen },

			.numTunes = numTunes,
			.flags = flags,
			.startTune = startTune,

		} ).first->second;

		ent.init ( tunePropPtr, dstToUse );
		if ( numTunes > maxTunesArray )
			dstToUse += ( numTunes - 1 ) * usid::wordsPerSubtune;
	}

	return hvscVersion;
}
//-----------------------------------------------------------------------------

void Database::applyOverrides ( const libsidplayEZ::OverrideSelector::overrideMap& overMap )
{
	//
	// Apply overrides
	//
	auto applyOverride = [] ( entry& ent, const libsidplayEZ::OverrideSelector::overrides& over )
	{
		// Start song
		if ( over.startTune )
			ent.startTune = over.startTune;

		// Clock
		if ( ! ( ent.flags & 0x000C ) && over.clock )
			ent.flags |= uint16_t ( over.clock << 2 );

		// SID-model
		if ( over.chipModel )
			ent.flags = ( ent.flags & ~0x0030 ) | uint16_t ( over.chipModel << 4 );
	};

	for ( const auto& overEntry : overMap )
	{
		const auto	dbTunePath = std::string ( filepaths::hvscMarker ) + overEntry.tune;

		if ( dbTunePath.ends_with ( "/" ) )
		{
			// Apply to all tunes with this prefix
			for ( auto& [ dbPath, dbEntry ] : db )
				if ( dbPath.starts_with ( dbTunePath ) )
					applyOverride ( dbEntry, overEntry );
		}
		else
		{
			// Apply to single tune only
			if ( auto it = db.find ( dbTunePath ); it != db.end () )
				applyOverride ( it->second, overEntry);
		}
	}
}
//-----------------------------------------------------------------------------

const Database::entry* Database::entryForSong ( const std::string& filename, const unsigned int songNo ) const
{
	return songNo ? findEntry ( filename ) : nullptr;
}
//-----------------------------------------------------------------------------

float Database::getSongLoudness ( const std::string& filename, unsigned int songNo ) const
{
	const auto	ent = entryForSong ( filename, songNo );

	return ent ? ent->getLoudness ( songNo - 1 ) : -96.0f;
}
//-----------------------------------------------------------------------------

float Database::getSongMidLoudness ( const std::string& filename, unsigned int songNo ) const
{
	const auto	ent = entryForSong ( filename, songNo );

	return ent ? ent->getMidLoudness ( songNo - 1 ) : -96.0f;
}
//-----------------------------------------------------------------------------

bool Database::getSongFilterUsed ( const std::string& filename, unsigned int songNo ) const
{
	const auto	ent = entryForSong ( filename, songNo );

	return ent ? ent->hasFilter ( songNo - 1 ) : true;
}
//-----------------------------------------------------------------------------

bool Database::getSongDigiUsed ( const std::string& filename, unsigned int songNo ) const
{
	const auto	ent = entryForSong ( filename, songNo );

	return ent ? ent->hasDigi ( songNo - 1 ) : false;
}
//-----------------------------------------------------------------------------

bool Database::getSongIsOneShot ( const std::string& filename, unsigned int songNo ) const
{
	const auto	ent = entryForSong ( filename, songNo );

	return ent ? ent->hasOneShot ( int ( songNo ) - 1 ) : false;
}
//-----------------------------------------------------------------------------

int Database::getVersion () const
{
	return hvscVersion;
}
//-----------------------------------------------------------------------------

std::vector<const Database::entry*> Database::getAllEntries ()
{
	std::vector<const Database::entry*>	vec;

	vec.reserve ( db.size () );

	for ( const auto& ent : db )
		vec.emplace_back ( &ent.second );

	std::ranges::sort ( vec, [] ( const entry* a, const entry* b ) {
		return lime::str::naturalCompare ( a->lowerName, b->lowerName ) < 0;
	} );

	return vec;
}
//-----------------------------------------------------------------------------

const Database::entry* Database::findEntry ( const std::string& hvscPath ) const
{
	if ( hvscPath.empty () )
		return nullptr;

	if ( auto it = db.find ( hvscPath ); it != db.end () )
		return &it->second;

	return nullptr;
}
//-----------------------------------------------------------------------------

void UserDatabase::scanUserTunes ()
{
	db.clear ();
	backing.clear ();

	auto	path = filepaths::getUserTunesPath ();
	if ( path == juce::File () )
		return;

	auto	tunesAsFileArray = path.findChildFiles ( juce::File::findFiles | juce::File::ignoreHiddenFiles, false, "*.sid" );

	// Open each file, get required information and store in database
	for ( const auto& file : tunesAsFileArray )
		addUserTune ( file );
}
//-----------------------------------------------------------------------------

std::string UserDatabase::getKey ( const juce::File& file )
{
	const auto	tname = file.getRelativePathFrom ( filepaths::getUserTunesPath () ).replaceCharacter ( '\\', '/' );

	return std::string ( "$USER$/" ) + tname.toStdString ();
}
//-----------------------------------------------------------------------------

void UserDatabase::addUserTune ( const juce::File& file )
{
	juce::FileInputStream	in ( file );
	if ( ! in.openedOk () )
		return;

	using namespace libsidplayfp;

	// Everything of interest sits in the header, up to and including the flags word
	constexpr auto	bytesNeeded = psid_headerSize + sizeof ( psidHeader::flags );

	juce::MemoryBlock	destBlock;
	auto	readSize = in.readIntoMemoryBlock ( destBlock, bytesNeeded );
	if ( readSize < 0x58 )	// minimum size for v1 files
		return;

	const auto	key = UserDatabase::getKey ( file );

//	auto getChar = [ &destBlock ] ( int offset ) {	char c; destBlock.copyTo ( &c, offset, 1 ); return c; };
	auto getWORD = [ &destBlock ] ( int offset ) {	uint16_t w; destBlock.copyTo ( &w, offset, 2 ); return uint16_t ( ( w >> 8 ) + ( w << 8 ) ); };
//	auto getLONGWORD = [ &destBlock ] ( int offset ) {	uint32_t l; destBlock.copyTo ( &l, offset, 4 ); return l; };
//	auto	magic = getChar ( 0x0 );		// PSID or RSID
	auto	version = uint8_t ( getWORD ( 0x4 ) );

	// Only v2+ headers carry the flags word, and a shorter file is truncated
	if ( version >= 2 && readSize < int ( bytesNeeded ) )
		return;

	auto	play = getWORD ( 0x0C );
	auto	numTunes = getWORD ( 0x0E );
	auto	start = getWORD ( 0x10 );
//	auto	speed = getLONGWORD ( 0x12 );
	auto	flags = ( version >= 2 ) ? getWORD ( psid_headerSize ) : uint16_t ( 0 );

	auto getHeaderStr = [ &destBlock ] ( int offset )
	{
		char	buf[ 33 ] = {};
		destBlock.copyTo ( buf, offset, 32 );
		return std::string ( buf );
	};

	// Re-adding a known tune: the old entry aliases the backing, drop it first
	db.erase ( key );

	auto&	[ storedKey, bck ] = *backing.try_emplace ( key ).first;
	bck = { getHeaderStr ( 0x16 ), getHeaderStr ( 0x36 ), getHeaderStr ( 0x56 ), {} };

	auto&	ent = db[ std::string_view ( storedKey ) ];

	ent = {

		.file = storedKey,
		.author = bck.author,
		.release = bck.release,

		.numTunes = numTunes,
		.flags = flags,
		.startTune = start,
		.userTune = true,
	};

	ent.initUser ( play ? 0xD : 0xF );

	resolveNames ();
}
//-----------------------------------------------------------------------------

void UserDatabase::removeUserTune ( const juce::File& file )
{
	const auto	key = UserDatabase::getKey ( file );

	// The entry's views alias the backing, so the entry goes first
	db.erase ( key );
	backing.erase ( key );

	resolveNames ();
}
//-----------------------------------------------------------------------------

// Placeholder ("<?>") titles and duplicate names show the filename instead;
// the whole user database counts as one folder
void UserDatabase::resolveNames ()
{
	std::unordered_map<std::string, int>	counts;

	for ( const auto& [ key, bck ] : backing )
		if ( ! tunenames::isPlaceholder ( bck.name ) )
			++counts[ tunenames::folded ( bck.name ) ];

	for ( auto& [ key, bck ] : backing )
	{
		const auto	shown = ( tunenames::isPlaceholder ( bck.name ) || counts[ tunenames::folded ( bck.name ) ] > 1 )
							? tunenames::stemName ( key ) : bck.name;

		// texts: the shown name, then the folded search line in arena layout
		auto&	t = bck.texts;

		t.clear ();
		t.reserve ( shown.size () + key.size () + shown.size () + bck.author.size () + bck.release.size () + 3 );

		t += shown;
		const auto	lineOff = t.size ();
		t += key;			t += '\0';
		t += shown;			t += '\0';
		t += bck.author;	t += '\0';
		t += bck.release;

		foldCorpus ( t.data () + lineOff, key.size (), asciiLowerLut.data () );
		foldCorpus ( t.data () + lineOff + key.size () + 1, t.size () - lineOff - key.size () - 1, sortingLut );

		auto	it = db.find ( std::string_view ( key ) );
		if ( it == db.end () )
		{
			Z_ERR ( "User tune has backing but no entry: " << key );
			continue;
		}

		auto&	ent = it->second;
		const auto	base = std::string_view ( t );

		ent.name = base.substr ( 0, shown.size () );
		ent.search = base.substr ( lineOff );
		ent.lowerFile = base.substr ( lineOff, key.size () );
		ent.lowerName = base.substr ( lineOff + key.size () + 1, shown.size () );
		ent.lowerRelease = base.substr ( t.size () - bck.release.size () );
	}
}
//-----------------------------------------------------------------------------

[[ nodiscard ]] int16_t Database::entry::getProperties ( const int songNo ) const
{
	if ( songNo < 0 || songNo >= numTunes )
		return 0xF;

	if ( numTunes > maxTunesArray )
	{
		if ( userTune )
			return 0xF;

		// The array tail stores the first song's pair: it's a union between a
		// pointer and an array, the tail is unused by the pointer and thus can
		// safely serve as storage
		if ( songNo == 0 )
			return tuneProperties.propsArr[ arraySlots - 2 ];

		return tuneProperties.propsPtr[ ( songNo - 1 ) * usid::wordsPerSubtune ];
	}

	return tuneProperties.propsArr[ songNo * usid::wordsPerSubtune ];
}
//-----------------------------------------------------------------------------

[[ nodiscard ]] int16_t Database::entry::getMidWord ( const int songNo ) const
{
	if ( songNo < 0 || songNo >= numTunes )
		return 0;

	if ( numTunes > maxTunesArray )
	{
		if ( userTune )
			return 0;

		if ( songNo == 0 )
			return tuneProperties.propsArr[ arraySlots - 1 ];

		return tuneProperties.propsPtr[ ( songNo - 1 ) * usid::wordsPerSubtune + 1 ];
	}

	return tuneProperties.propsArr[ songNo * usid::wordsPerSubtune + 1 ];
}
//-----------------------------------------------------------------------------

[[ nodiscard ]] float Database::entry::getLoudness ( const int songNo ) const
{
	return usid::getLoudness ( getProperties ( songNo ) );
}
//-----------------------------------------------------------------------------

[[ nodiscard ]] float Database::entry::getMidLoudness ( const int songNo ) const
{
	return usid::getMidLoudness ( getMidWord ( songNo ) );
}
//-----------------------------------------------------------------------------

bool Database::entry::hasAnyFlag ( const int flag ) const
{
	if ( numTunes > maxTunesArray )
	{
		if ( tuneProperties.propsArr[ arraySlots - 2 ] & flag )
			return true;

		if ( userTune )
			return true;

		for ( auto i = 0; i < ( numTunes - 1 ); ++i )
			if ( tuneProperties.propsPtr[ i * usid::wordsPerSubtune ] & flag )
				return true;

		return false;
	}

	for ( auto i = 0; i < numTunes; ++i )
		if ( tuneProperties.propsArr[ i * usid::wordsPerSubtune ] & flag )
			return true;

	return false;
}
//-----------------------------------------------------------------------------

const Database::entry* db::findDatabaseEntry ( const std::string& filename )
{
	const juce::SharedResourcePointer<Database>	database;

	if ( auto ent = database->findEntry ( filename ) )
		return ent;

	const juce::SharedResourcePointer<UserDatabase>	userDatabase;
	return userDatabase->findEntry ( filename );
}
//-----------------------------------------------------------------------------
