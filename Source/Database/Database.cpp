#include <array>

#include "Database.h"

#include "std_lime/lime_string_utils.h"

#include "Config/FilePaths.h"
#include "Database/TuneInfo.h"
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

// The search form of a folded character: path separators become a space,
// anything but letters, digits and spaces is dropped (0)
static constexpr char searchChar ( const char c )
{
	if ( c == '_' || c == '/' )
		return ' ';

	return ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == ' ' ? c : 0;
}

static char searchFold ( const char c )
{
	return searchChar ( char ( sortingLut[ uint8_t ( c ) ] ) );
}

size_t db::foldSearch ( const std::string_view text, char* const dst, textSpan* const sources )
{
	static constexpr std::pair<std::string_view, char>	numerals[] =
	{
		{ "ii", '2' }, { "iii", '3' }, { "iv", '4' }, { "vi", '6' }, { "vii", '7' }, { "viii", '8' }, { "ix", '9' },
	};

	size_t	n = 0;
	size_t	wordStart = 0;
	char	word[ 4 ];
	auto	wordLen = 0;		// -1 once the word can no longer be a numeral

	const auto	emit = [ & ] ( const char c, const textSpan source )
	{
		if ( dst )
			dst[ n ] = c;
		if ( sources )
			sources[ n ] = source;
		++n;
	};

	const auto	endWord = [ & ]
	{
		if ( wordLen > 0 )
			for ( const auto& [ numeral, digit ] : numerals )
				if ( numeral == std::string_view ( word, size_t ( wordLen ) ) )
				{
					const auto	source = sources ? textSpan { sources[ wordStart ].from, sources[ n - 1 ].to } : textSpan {};

					n = wordStart;
					emit ( digit, source );
					break;
				}

		wordLen = 0;
	};

	for ( size_t i = 0; i < text.size (); ++i )
	{
		const auto	f = searchFold ( text[ i ] );
		if ( ! f )
			continue;

		const auto	source = textSpan { int ( i ), int ( i + 1 ) };

		if ( f == ' ' )
		{
			endWord ();
			emit ( ' ', source );
			wordStart = n;
			continue;
		}

		if ( wordLen >= 0 )
		{
			if ( wordLen < 4 && ( f == 'i' || f == 'v' || f == 'x' ) )
				word[ wordLen++ ] = f;
			else
				wordLen = -1;
		}

		emit ( f, source );
	}

	endWord ();

	return n;
}

static size_t strippedSize ( const std::string_view s )
{
	return db::foldSearch ( s, nullptr );
}

static bool strippedWhole ( const std::string_view s )
{
	return strippedSize ( s ) == s.size () && std::ranges::all_of ( s, [] ( const char c ) { return searchFold ( c ) == char ( sortingLut[ uint8_t ( c ) ] ); } );
}
//-----------------------------------------------------------------------------

// A name that foldSearch leaves whole is not copied, its search view
// aliases the sorting fold
struct foldedViews
{
	std::string_view	lowerFile;
	std::string_view	lowerName;
	std::string_view	searchFile;
	std::string_view	searchName;
	std::string_view	searchAuthor;
	std::string_view	searchPublisher;
};

static size_t foldedBytes ( const size_t keyLen, const std::string_view path, const std::string_view name, const std::string_view author, const std::string_view publisher )
{
	return keyLen + name.size () + strippedSize ( path ) + ( strippedWhole ( name ) ? 0 : strippedSize ( name ) ) + strippedSize ( author ) + strippedSize ( publisher );
}

static foldedViews writeFolded ( char*& dst, const std::string_view key, const std::string_view path, const std::string_view name, const std::string_view author, const std::string_view publisher )
{
	const auto	putFolded = [ &dst ] ( const std::string_view s, const uint8_t* const lut )
	{
		const auto	start = dst;

		for ( const auto c : s )
			*dst++ = char ( lut[ uint8_t ( c ) ] );

		return std::string_view ( start, size_t ( dst - start ) );
	};

	const auto	putStripped = [ &dst ] ( const std::string_view s )
	{
		const auto	start = dst;

		dst += db::foldSearch ( s, dst );

		return std::string_view ( start, size_t ( dst - start ) );
	};

	foldedViews	views;

	views.lowerFile = putFolded ( key, asciiLowerLut.data () );
	views.lowerName = putFolded ( name, sortingLut );
	views.searchFile = putStripped ( path );
	views.searchName = strippedWhole ( name ) ? views.lowerName : putStripped ( name );
	views.searchAuthor = putStripped ( author );
	views.searchPublisher = putStripped ( publisher );

	return views;
}
//-----------------------------------------------------------------------------

// The release after its "YYYY " year, the part that is searchable text
static std::string_view releasePublisher ( const std::string_view release )
{
	const auto	isYearChar = [] ( const char c ) { return ( c >= '0' && c <= '9' ) || c == '?'; };

	if ( release.size () < 4 || ! std::ranges::all_of ( release.substr ( 0, 4 ), isYearChar ) )
		return release;

	auto	rest = release.substr ( 4 );

	if ( rest.starts_with ( ' ' ) )
		rest.remove_prefix ( 1 );

	return rest;
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
			const auto	path = std::string_view ( (const char*)tempSrc, fileLen );
			tempSrc += fileLen;

			std::string_view	texts[ 3 ];		// name, author, release
			size_t				textLen = 0;
			for ( auto& text : texts )
			{
				const auto	len = size_t ( *tempSrc++ );
				text = { (const char*)tempSrc, len };
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

			// "$HVSC$<path>.sid" + the three text fields
			stringBytes += 6 + fileLen + 4 + textLen;
			corpusBytes += foldedBytes ( 6 + fileLen + 4, path, texts[ 0 ], texts[ 1 ], releasePublisher ( texts[ 2 ] ) );
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

		const auto	nameLen = size_t ( name.second ), authorLen = size_t ( author.second ), releaseLen = size_t ( release.second );

		const auto	nameStart = sp;		put ( sp, name.first, nameLen );
		const auto	authorStart = sp;	put ( sp, author.first, authorLen );
		const auto	releaseStart = sp;	put ( sp, release.first, releaseLen );

		const auto	views = writeFolded ( cp, { keyStart, keyLen }, { path.first, size_t ( path.second ) }, { nameStart, nameLen }, { authorStart, authorLen },
										  releasePublisher ( { releaseStart, releaseLen } ) );

		auto&	ent = db.emplace ( std::string_view ( keyStart, keyLen ), entry {

			.file = { keyStart, keyLen },
			.name = { nameStart, nameLen },
			.author = { authorStart, authorLen },
			.release = { releaseStart, releaseLen },

			.lowerFile = views.lowerFile,
			.lowerName = views.lowerName,
			.searchFile = views.searchFile,
			.searchName = views.searchName,
			.searchAuthor = views.searchAuthor,
			.searchPublisher = views.searchPublisher,

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

		const auto	publisher = releasePublisher ( bck.release );

		auto	path = filepaths::stripLocationMarker ( std::string_view ( key ) );
		if ( const auto dot = path.rfind ( '.' ); dot != std::string_view::npos )
			path = path.substr ( 0, dot );

		auto&	t = bck.texts;

		t = shown;
		t.resize ( shown.size () + foldedBytes ( key.size (), path, shown, bck.author, publisher ) );

		auto		dst = t.data () + shown.size ();
		const auto	views = writeFolded ( dst, key, path, shown, bck.author, publisher );

		auto	it = db.find ( std::string_view ( key ) );
		if ( it == db.end () )
		{
			Z_ERR ( "User tune has backing but no entry: " << key );
			continue;
		}

		auto&	ent = it->second;

		ent.name = std::string_view ( t ).substr ( 0, shown.size () );
		ent.lowerFile = views.lowerFile;
		ent.lowerName = views.lowerName;
		ent.searchFile = views.searchFile;
		ent.searchName = views.searchName;
		ent.searchAuthor = views.searchAuthor;
		ent.searchPublisher = views.searchPublisher;
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

// A "_Nsid.sid" filename declares N chips
int Database::entry::sidCount () const
{
	const auto	underscore = lowerFile.rfind ( '_' );
	if ( underscore == std::string_view::npos )
		return 1;

	auto	digits = lowerFile.substr ( underscore + 1 );
	if ( ! digits.ends_with ( "sid.sid" ) )
		return 1;

	digits.remove_suffix ( 7 );

	auto	count = 0;
	for ( const auto c : digits )
	{
		if ( c < '0' || c > '9' )
			return 1;

		count = count * 10 + ( c - '0' );
	}

	return std::max ( count, 1 );
}
//-----------------------------------------------------------------------------

int Database::entry::chipModels () const
{
	const auto	sid1 = ( flags >> 4 ) & 3;
	const auto	sid2 = ( flags >> 6 ) & 3;
	const auto	sid3 = ( flags >> 8 ) & 3;

	if ( sidCount () > 1 )
		return sid1 | sid2 | sid3;

	return sid1 == 3 ? 1 : sid1;
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
//-----------------------------------------------------------------------------

db::SortItem db::sortItem ( const SortKey key, const Database::entry* entry, const int subtune )
{
	if ( key != SortKey::length || ! entry )
		return { entry };

	return { entry, SID::getTuneLength ( entry->file, realSubtune ( entry, subtune ) ) };
}
//-----------------------------------------------------------------------------

bool db::entryLess ( const SortKey key, const bool forwards, const SortItem& ia, const SortItem& ib )
{
	if ( ! ia.entry || ! ib.entry )
		return ia.entry && ! ib.entry;

	const auto&	a = *ia.entry;
	const auto&	b = *ib.entry;

	const auto	nameLess = [ & ] { return lime::str::naturalCompare ( a.lowerName, b.lowerName ) < 0; };
	const auto	yearCompare = [ & ] { return a.release.substr ( 0, 4 ).compare ( b.release.substr ( 0, 4 ) ); };

	switch ( key )
	{
		case SortKey::name:
		{
			const auto	cmp = lime::str::naturalCompare ( a.lowerName, b.lowerName );
			return forwards ? cmp < 0 : cmp > 0;
		}

		case SortKey::release:
		{
			const auto	cmp = yearCompare ();
			if ( ! cmp )
				return nameLess ();

			return forwards ? cmp < 0 : cmp > 0;
		}

		case SortKey::chip:
		{
			const auto	fA = a.flags & 0x30;
			const auto	fB = b.flags & 0x30;

			if ( fA == fB )
			{
				const auto	cmp = yearCompare ();
				if ( ! cmp )
					return nameLess ();

				return forwards ? cmp < 0 : cmp > 0;
			}

			return forwards ? fA < fB : fB < fA;
		}

		case SortKey::length:
		{
			if ( ia.lengthMs == ib.lengthMs )
				return nameLess ();

			return forwards ? ia.lengthMs < ib.lengthMs : ia.lengthMs > ib.lengthMs;
		}

		case SortKey::none:
			break;
	}

	return false;
}
//-----------------------------------------------------------------------------
