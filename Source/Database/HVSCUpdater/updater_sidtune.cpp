#include <algorithm>
#include <cstdint>
#include <cstring>

#include "updater_sidtune.h"

using namespace libsidplayfp;

// The flags word holds one 2-bit model field per SID chip. PSID_SIDMODEL masks the
// first, the others sit two and four bits above it
constexpr auto	PSID_SIDMODEL2 = PSID_SIDMODEL << 2;
constexpr auto	PSID_SIDMODEL3 = PSID_SIDMODEL << 4;

// PSID_SIDMODEL_* are the values of a model field once shifted down, this puts one back
constexpr auto	psidSidModelShift = 4;

constexpr auto	text_emptyFile = "ERROR: File is empty";
constexpr auto	text_unrecognizedFormat = "ERROR: Could not determine file format";
constexpr auto	text_fileIoError = "ERROR: File I/O error";
constexpr auto	text_noErrors = "No errors";
constexpr auto	text_na = "N/A";

//-----------------------------------------------------------------------------

updater_sidTune::updater_sidTune ( const juce::MemoryBlock& data )
{
	// Initialize the object with some safe defaults
	info.statusString = text_na;
	info.dataFileLen = info.c64dataLen = 0;
	info.formatString = text_na;
	info.speedString = text_na;
	info.loadAddr = ( info.initAddr = ( info.playAddr = 0 ) );
	info.songs = ( info.startSong = ( info.currentSong = 0 ) );
	info.musPlayer = false;
	info.compatibility = SIDTUNE_COMPATIBILITY_C64;
	info.sidModel = PSID_SIDMODEL_UNKNOWN;
	info.fixLoad = false;
	info.songSpeed = SIDTUNE_SPEED_VBI;
	info.clockSpeed = PSID_CLOCK_UNKNOWN;
	info.lengthInSeconds = 0;
	info.relocStartPage = 0;
	info.relocPages = 0;

	std::fill_n ( songSpeed, classMaxSongs, SIDTUNE_SPEED_VBI );
	std::fill_n ( clockSpeed, classMaxSongs, PSID_CLOCK_PAL );
	std::fill_n ( songLength, classMaxSongs, 0 );

	info.numberOfInfoStrings = 0;

	// Load file
	if ( info.dataFileLen = loadData ( data ); info.dataFileLen )
	{
		if ( PSID_fileSupport ( fileBuf.data (), info.dataFileLen ) )
			acceptSidTune ( fileBuf.data (), info.dataFileLen );
		else
		{
			info.formatString = text_na;
			info.statusString = text_unrecognizedFormat;
			status = false;
		}
	}
	else
	{
		info.formatString = text_na;
		status = false;
	}
}
//-----------------------------------------------------------------------------

void updater_sidTune::fixLoadAddress ( bool force, uint16_t init, uint16_t play )
{
	if ( info.fixLoad || force )
	{
		info.fixLoad = false;

		if ( fileOffset + 2 <= info.dataFileLen )
		{
			info.loadAddr += 2;
			fileOffset += 2;
		}

		if ( force )
		{
			info.initAddr = init;
			info.playAddr = play;
		}
	}
}
//-----------------------------------------------------------------------------

uint32_t updater_sidTune::loadData ( const juce::MemoryBlock& data )
{
	status = false;

	const auto	fileLen = uint32_t ( data.getSize () );
	if ( ! fileLen )
	{
		info.statusString = text_emptyFile;
		return 0;
	}

	fileBuf.resize ( fileLen + 1 );
	fileBuf[ fileLen ] = 0;

	std::memcpy ( fileBuf.data (), data.getData (), fileLen );

	status = true;
	info.statusString = text_noErrors;

	return fileLen;
}
//-----------------------------------------------------------------------------

bool updater_sidTune::cacheRawData ( const uint8_t* sourceBuf, uint32_t sourceBufLen )
{
	cachedData.assign ( sourceBuf, sourceBuf + sourceBufLen );

	if ( fileOffset + 2 <= sourceBufLen )
	{
		// We only detect an offset of two. Some position independent
		// sidtunes contain a load address of 0xE000, but are loaded
		// to 0x0FFE and call player at 0x1000.
		auto readLEword = [] ( const uint8_t ptr[ 2 ] ) -> uint16_t
		{
			return uint16_t ( ptr[ 1 ] << 8 | ptr[ 0 ] );
		};

		info.fixLoad = ( readLEword ( (const uint8_t*)sourceBuf + fileOffset ) == ( info.loadAddr + 2 ) );
	}

	// Caching cannot vindicate a load that already failed, so status stays as it is
	return true;
}
//-----------------------------------------------------------------------------

void updater_sidTune::acceptSidTune ( const uint8_t* dataBuf, uint32_t dataLen )
{
	// Fix bad sidtune set up
	info.songs = std::clamp ( info.songs, uint16_t ( 1 ), uint16_t ( classMaxSongs ) );

	if ( info.startSong == 0 || info.startSong > info.songs )
		info.startSong = 1;

	info.dataFileLen = dataLen;
	info.c64dataLen = dataLen - fileOffset;

	cacheRawData ( dataBuf, dataLen );
}
//-----------------------------------------------------------------------------

void updater_sidTune::convertOldStyleSpeedToTables ( uint32_t oldStyleSpeed, int clock )
{
	// Create the speed/clock setting tables
	//
	// This does not take into account the PlaySID bug upon evaluating the
	// SPEED field. It would most likely break compatibility to lots of
	// sidtunes, which have been converted from .SID format and vice versa.
	// The .SID format does the bit-wise/song-wise evaluation of the SPEED
	// value correctly, like it is described in the PlaySID documentation.
	auto	toDo = int ( ( info.songs <= classMaxSongs ) ? info.songs : classMaxSongs );
	for ( auto s = 0; s < toDo; ++s )
	{
		clockSpeed[ s ] = uint8_t ( clock );
		if ( ( ( oldStyleSpeed >> ( s & 31 ) ) & 1 ) == 0 )
			songSpeed[ s ] = SIDTUNE_SPEED_VBI;
		else
			songSpeed[ s ] = SIDTUNE_SPEED_CIA_1A;
	}
}
//-----------------------------------------------------------------------------

juce::MemoryBlock updater_sidTune::savePSIDData ()
{
	// This prevents saving from a bad object
	if ( ! status )
		return {};

	juce::MemoryOutputStream	out;

	if ( ! PSID_fileSupportSave ( out, cachedData.data () ) )
	{
		info.statusString = text_fileIoError;
		return {};
	}

	info.statusString = text_noErrors;
	return out.getMemoryBlock ();
}
//-----------------------------------------------------------------------------

bool updater_sidTune::checkRealC64Info ( uint32_t speed )
{
	if ( info.loadAddr != 0 )		return false;
	if ( info.playAddr != 0 )		return false;
	if ( speed != 0 )				return false;

	if ( info.compatibility == SIDTUNE_COMPATIBILITY_BASIC )
		if ( info.initAddr != 0 )	return false;

	return true;
}
//-----------------------------------------------------------------------------

bool updater_sidTune::checkCompatibility ( void )
{
	switch ( info.compatibility )
	{
		case SIDTUNE_COMPATIBILITY_R64:
			// Check valid init address
			switch ( info.initAddr >> 12 )
			{
				case 0x0F:
				case 0x0E:
				case 0x0D:
				case 0x0B:
				case 0x0A:
					return false;
				default:
					if ( ( info.initAddr < info.loadAddr ) ||
						 ( info.initAddr > ( info.loadAddr + info.c64dataLen - 1 ) ) )
					{
						return false;
					}
			}
			// deliberate run on

		case SIDTUNE_COMPATIBILITY_BASIC:
			// Check tune is loadable on a real C64
			if ( info.loadAddr < 0x07e8 )
				return false;
			if ( info.playAddr != 0 )
				return false;
			if ( info.compatibility == SIDTUNE_COMPATIBILITY_R64 )
				break;
			if ( info.initAddr != 0 )
				return false;
			break;
	}
	return true;
}
//-----------------------------------------------------------------------------

bool updater_sidTune::checkRelocInfo ( void )
{
	uint8_t startp, endp;

	// Fix relocation information
	if ( info.relocStartPage == 0xFF )
	{
		info.relocPages = 0;
		return true;
	}
	else if ( info.relocPages == 0 )
	{
		info.relocStartPage = 0;
		return true;
	}

	// Calculate start/end page
	startp = info.relocStartPage;
	endp = ( startp + info.relocPages - 1 ) & 0xff;
	if ( endp < startp )
		return false;

	{	// Check against load range
		uint8_t startlp, endlp;
		startlp = (uint8_t)( info.loadAddr >> 8 );
		endlp = startlp;
		endlp += (uint8_t)( ( info.c64dataLen - 1 ) >> 8 );

		if ( ( ( startp <= startlp ) && ( endp >= startlp ) ) ||
			 ( ( startp <= endlp ) && ( endp >= endlp ) ) )
		{
			return false;
		}
	}

	// Check that the relocation information does not use the following
	// memory areas: 0x0000-0x03FF, 0xA000-0xBFFF and 0xD000-0xFFFF
	if ( ( startp < 0x04 )
		 || ( ( 0xa0 <= startp ) && ( startp <= 0xbf ) )
		 || ( startp >= 0xd0 )
		 || ( ( 0xa0 <= endp ) && ( endp <= 0xbf ) )
		 || ( endp >= 0xd0 ) )
	{
		return false;
	}
	return true;
}
//-----------------------------------------------------------------------------

// Header has been extended for 'RSID' format
// The following changes are present:
//     id = 'RSID'
//     version = 2 only
//     play, load and speed reserved 0
//     psidspecific flag reserved 0
//     init cannot be under ROMS/IO
//     load cannot be less than 0x0801 (start of basic)

struct updater_psidHeader
{
	//
	// All values in big-endian order.
	//
	char	id[ 4 ];          // 'PSID'
	uint8_t version[ 2 ];    // 0x0001 or 0x0002
	uint8_t data[ 2 ];       // 16-bit offset to binary data in file
	uint8_t load[ 2 ];       // 16-bit C64 address to load file to
	uint8_t init[ 2 ];       // 16-bit C64 address of init subroutine
	uint8_t play[ 2 ];       // 16-bit C64 address of play subroutine
	uint8_t songs[ 2 ];      // number of songs
	uint8_t start[ 2 ];      // start song (1-256 !)
	uint8_t speed[ 4 ];      // 32-bit speed info
	// bit: 0=50 Hz, 1=CIA 1 Timer A (default: 60 Hz)
	char	name[ 32 ];       // ASCII strings, 31 characters long and
	char	author[ 32 ];     // terminated by a trailing zero
	char	copyright[ 32 ];  //
	uint8_t flags[ 2 ];             // only version 0x0002
	uint8_t relocStartPage[ 1 ];    // only version 0x0002
	uint8_t relocPages[ 1 ];        // only version 0x0002
	uint8_t reserved[ 2 ];          // only version 0x0002, used from version 3
};


static const char _sidtune_format_psid[] = "PlaySID one-file format (PSID)";
static const char _sidtune_format_rsid[] = "Real C64 one-file format (RSID)";
static const char _sidtune_unknown[] = "Unsupported file format";
static const char _sidtune_unknown_psid[] = "Unsupported PSID version";
static const char _sidtune_unknown_rsid[] = "Unsupported RSID version";
static const char _sidtune_truncated[] = "ERROR: File is most likely truncated";
static const char _sidtune_invalid[] = "ERROR: File contains invalid data";
static const char _sidtune_reloc[] = "ERROR: File contains bad reloc data";

constexpr auto	_sidtune_psid_maxStrLen = 32;

//-----------------------------------------------------------------------------

inline uint32_t readBEdword ( const uint8_t ptr[ 4 ] )
{
	return uint32_t ( ptr[ 0 ] << 24 | ptr[ 1 ] << 16 | ptr[ 2 ] << 8 | ptr[ 3 ] );
}
//-----------------------------------------------------------------------------

inline uint16_t readBEword ( const uint8_t ptr[ 2 ] )
{
	return uint16_t ( ptr[ 0 ] << 8 | ptr[ 1 ] );
}
//-----------------------------------------------------------------------------

inline void writeBEword ( uint8_t ptr[ 2 ], uint16_t someWord )
{
	ptr[ 0 ] = someWord >> 8;
	ptr[ 1 ] = someWord & 0xFF;
}
//-----------------------------------------------------------------------------

inline void writeBEdword ( uint8_t ptr[ 4 ], uint32_t someDword )
{
	ptr[ 0 ] = someDword >> 24;
	ptr[ 1 ] = ( someDword >> 16 ) & 0xFF;
	ptr[ 2 ] = ( someDword >> 8 ) & 0xFF;
	ptr[ 3 ] = someDword & 0xFF;
}
//-----------------------------------------------------------------------------

inline uint16_t readEndian ( uint8_t hi, uint8_t lo )
{
	return uint16_t ( hi << 8 | lo );
}
//-----------------------------------------------------------------------------

bool updater_sidTune::PSID_fileSupport ( const void* buffer, uint32_t bufLen )
{
	int	clock = PSID_CLOCK_UNKNOWN;
	int	compatibility = SIDTUNE_COMPATIBILITY_C64;

	// Require minimum size to allow access to the first few bytes.
	// Require a valid ID and version number.
	const auto	pHeader = (const updater_psidHeader*)buffer;

	// Remove any format description or format error string.
	info.formatString = 0;

	// File format check
	if ( bufLen < 6 )
		return false;

	if ( readBEdword ( (const uint8_t*)pHeader->id ) == PSID_ID )
	{
		if ( readBEword ( pHeader->version ) > 4 )
		{
			info.formatString = _sidtune_unknown_psid;
			return false;
		}
		info.formatString = _sidtune_format_psid;
	}
	else if ( readBEdword ( (const uint8_t*)pHeader->id ) == RSID_ID )
	{
		if ( ( readBEword ( pHeader->version ) < 2 ) || ( readBEword ( pHeader->version ) > 4 ) )
		{
			info.formatString = _sidtune_unknown_rsid;
			return false;
		}
		info.formatString = _sidtune_format_rsid;
		compatibility = SIDTUNE_COMPATIBILITY_R64;
	}
	else
	{
		info.formatString = _sidtune_unknown;
		return false;
	}

	// Due to security concerns, input must be at least as long as version 1
	// header plus 16-bit C64 load address. That is the area which will be
	// accessed.
	if ( bufLen < ( sizeof ( updater_psidHeader ) + 2 ) )
	{
		info.formatString = _sidtune_truncated;
		return false;
	}

	fileOffset = readBEword ( pHeader->data );

	// The data offset must lie inside the file, with room for an embedded load address
	if ( fileOffset + 2u > bufLen )
	{
		info.formatString = _sidtune_truncated;
		return false;
	}

	info.loadAddr = readBEword ( pHeader->load );
	info.initAddr = readBEword ( pHeader->init );
	info.playAddr = readBEword ( pHeader->play );
	info.songs = readBEword ( pHeader->songs );
	info.startSong = readBEword ( pHeader->start );
	info.compatibility = compatibility;
	auto	speed = readBEdword ( pHeader->speed );
	// added for v3, convert to v2 only v1 headers, v3 must be kept
	info.version = readBEword ( pHeader->version );

	if ( info.version < 2 )				info.version = 2;
	if ( info.songs > classMaxSongs )	info.songs = classMaxSongs;

	info.musPlayer = false;
	info.sidModel = PSID_SIDMODEL_UNKNOWN;
	info.relocPages = 0;
	info.relocStartPage = 0;
	if ( readBEword ( pHeader->version ) >= 2 )
	{
		const auto	flags = readBEword ( pHeader->flags );

		if ( flags & PSID_MUS )
		{
			// MUS tunes run at any speed
			clock = PSID_CLOCK_ANY;
			info.musPlayer = true;
		}

		// This flags is only available for the appropriate
		// file formats
		switch ( compatibility )
		{
			case SIDTUNE_COMPATIBILITY_C64:
				if ( flags & PSID_SPECIFIC )
					info.compatibility = SIDTUNE_COMPATIBILITY_PSID;
				break;

			case SIDTUNE_COMPATIBILITY_R64:
				if ( flags & PSID_BASIC )
					info.compatibility = SIDTUNE_COMPATIBILITY_BASIC;
				break;
		}

		clock |= flags & PSID_CLOCK;
		info.clockSpeed = uint8_t ( clock );

		info.sidModel = flags & PSID_SIDMODEL;
		// added for v3
		if ( info.version >= 3 )
		{
			info.sidModel |= flags & PSID_SIDMODEL2;
			info.reserved = readBEword ( pHeader->reserved );
		}
		// added for v4
		if ( info.version == 4 )
		{
			info.sidModel |= flags & PSID_SIDMODEL3;
			info.reserved = readBEword ( pHeader->reserved );
		}

		info.relocStartPage = pHeader->relocStartPage[ 0 ];
		info.relocPages = pHeader->relocPages[ 0 ];
	}

	// Check reserved fields to force real c64 compliance
	if ( compatibility == SIDTUNE_COMPATIBILITY_R64 )
	{
		if ( ! checkRealC64Info ( speed ) )
		{
			info.formatString = _sidtune_invalid;
			return false;
		}
		// Real C64 tunes appear as CIA
		speed = ~0;
	}
	// Create the speed/clock setting table.
	convertOldStyleSpeedToTables ( speed, clock );

	if ( info.loadAddr == 0 )
	{
		auto	pData = (uint8_t*)buffer + fileOffset;
		info.loadAddr = readEndian ( *( pData + 1 ), *pData );
		fileOffset += 2;
	}

	// Obtain C64 data length now file header is fully
	// extracted
	info.c64dataLen = bufLen - fileOffset;

	if ( info.compatibility != SIDTUNE_COMPATIBILITY_BASIC )
	{
		if ( ! info.initAddr )
			info.initAddr = info.loadAddr;
	}

	if ( ! checkCompatibility () )
	{
		info.formatString = _sidtune_invalid;
		return false;
	}

	if ( ! checkRelocInfo () )
	{
		info.formatString = _sidtune_reloc;
		return false;
	}

	// Copy info strings, so they will not get lost
	info.numberOfInfoStrings = 3;

	// Name
	std::strncpy ( &infoString[ 0 ][ 0 ], pHeader->name, _sidtune_psid_maxStrLen );
	info.infoString[ 0 ] = &infoString[ 0 ][ 0 ];

	// Author
	std::strncpy ( &infoString[ 1 ][ 0 ], pHeader->author, _sidtune_psid_maxStrLen );
	info.infoString[ 1 ] = &infoString[ 1 ][ 0 ];

	// Copyright
	std::strncpy ( &infoString[ 2 ][ 0 ], pHeader->copyright, _sidtune_psid_maxStrLen );
	info.infoString[ 2 ] = &infoString[ 2 ][ 0 ];

	return true;
}
//-----------------------------------------------------------------------------

bool updater_sidTune::PSID_fileSupportSave ( juce::OutputStream& fMyOut, const uint8_t* dataBuffer )
{
	updater_psidHeader myHeader;

	writeBEdword ( (uint8_t*)myHeader.id, PSID_ID );
	writeBEword ( myHeader.version, info.version );
	writeBEword ( myHeader.data, sizeof ( updater_psidHeader ) );
	writeBEword ( myHeader.load, 0 );
	writeBEword ( myHeader.init, info.initAddr );
	writeBEword ( myHeader.play, info.playAddr );
	writeBEword ( myHeader.songs, info.songs );
	writeBEword ( myHeader.start, info.startSong );

	uint32_t	speed = 0;
	uint32_t	maxBugSongs = ( ( info.songs <= 32 ) ? info.songs : 32 );

	for ( auto s = 0u; s < maxBugSongs; s++ )
		if ( songSpeed[ s ] == SIDTUNE_SPEED_CIA_1A )
			speed |= ( 1 << s );

	writeBEdword ( myHeader.speed, speed );

	uint16_t	tmpFlags = 0;
	if ( info.musPlayer )											tmpFlags |= PSID_MUS;
	if ( info.compatibility == SIDTUNE_COMPATIBILITY_PSID )			tmpFlags |= PSID_SPECIFIC;
	else if ( info.compatibility == SIDTUNE_COMPATIBILITY_BASIC )	tmpFlags |= PSID_BASIC;

	tmpFlags |= info.clockSpeed;
	tmpFlags |= info.sidModel;

	writeBEword ( myHeader.flags, tmpFlags );

	myHeader.relocStartPage[ 0 ] = info.relocStartPage;
	myHeader.relocPages[ 0 ] = info.relocPages;

	writeBEword ( myHeader.reserved, 0 );

	std::strncpy ( myHeader.name, info.infoString[ 0 ], _sidtune_psid_maxStrLen );
	std::strncpy ( myHeader.author, info.infoString[ 1 ], _sidtune_psid_maxStrLen );
	std::strncpy ( myHeader.copyright, info.infoString[ 2 ], _sidtune_psid_maxStrLen );

	switch ( info.compatibility )
	{
		case SIDTUNE_COMPATIBILITY_BASIC:
			writeBEword ( myHeader.init, 0 );
			[[ fallthrough ]];

		case SIDTUNE_COMPATIBILITY_R64:
			writeBEdword ( (uint8_t*)myHeader.id, RSID_ID );
			writeBEword ( myHeader.play, 0 );
			writeBEdword ( myHeader.speed, 0 );
			break;
	}

	if ( info.version >= 3 )
		writeBEword ( myHeader.reserved, info.reserved );

	auto	ok = fMyOut.write ( &myHeader, sizeof ( updater_psidHeader ) );

	// Save C64 lo/hi load address (little-endian)
	{
		uint8_t	saveAddr[ 2 ];
		saveAddr[ 0 ] = info.loadAddr & 255;
		saveAddr[ 1 ] = info.loadAddr >> 8;
		ok = ok && fMyOut.write ( saveAddr, 2 );
	}

	// Data starts at: bufferaddr + fileoffset
	// Data length: datafilelen - fileoffset
	if ( info.dataFileLen > fileOffset )
		ok = ok && fMyOut.write ( dataBuffer + fileOffset, info.dataFileLen - fileOffset );

	return ok;
}
//-----------------------------------------------------------------------------

bool updater_sidTune::writeToSidTune ( char newInfoString[][ maxSidInfoLen + 1 ], mode_type mode )
{
	// PSID-format can only handle up to 31 characters plus a terminating zero.
	//
	// The infoStrings are kept without any special order in a two-dimensional
	// array of the size [infoStringNum][infoStringLen].
	//
	// To make available publically readable copies for each infoString,
	// extra pointers are assigned: info.nameString, info.authorString,
	// info.copyrightString, and for compatibility to MUS files and a future
	// file format: info.infoString[0], ..., info.infoString[infoStringNum].
	// A copy of the private instance of the sidTuneInfo structure can be read
	// out using ::returnInfo().

	auto	infoStringIndex = 0; // Used for FLAGS case.

	const auto	modeInt = int ( mode );

	switch ( mode )
	{
		case TITLE:
		case AUTHOR:
		case RELEASED:
		{
			// Copy string to private array.
			std::strcpy ( &infoString[ modeInt ][ 0 ], newInfoString[ modeInt ] );

			info.infoString[ modeInt ] = &infoString[ modeInt ][ 0 ];

			if ( mode == TITLE )		info.nameString = &infoString[ modeInt ][ 0 ];
			else if ( mode == AUTHOR )	info.authorString = &infoString[ modeInt ][ 0 ];
			else						info.copyrightString = &infoString[ modeInt ][ 0 ];

			break;
		}

		case CREDITS:
		{
			if ( '*' != newInfoString[ 0 ][ 0 ] )
			{
				std::strcpy ( &infoString[ 0 ][ 0 ], newInfoString[ 0 ] );
				info.nameString = &infoString[ 0 ][ 0 ];
				info.infoString[ 0 ] = &infoString[ 0 ][ 0 ];
			}
			if ( '*' != newInfoString[ 1 ][ 0 ] )
			{
				std::strcpy ( &infoString[ 1 ][ 0 ], newInfoString[ 1 ] );
				info.authorString = &infoString[ 1 ][ 0 ];
				info.infoString[ 1 ] = &infoString[ 1 ][ 0 ];
			}
			if ( '*' != newInfoString[ 2 ][ 0 ] )
			{
				std::strcpy ( &infoString[ 2 ][ 0 ], newInfoString[ 2 ] );
				info.copyrightString = &infoString[ 2 ][ 0 ];
				info.infoString[ 2 ] = &infoString[ 2 ][ 0 ];
			}
			break;
		}

		case SPEED:
		{
			// Not modifiable!
			if ( info.compatibility == SIDTUNE_COMPATIBILITY_R64 )
				return false;

			// SPEED string is in hex
			const auto	ulSpeed = std::strtoul ( newInfoString[ 0 ], nullptr, 16 );
			convertOldStyleSpeedToTables ( ulSpeed );
			break;
		}

		case SONGS:
		{
			if ( std::sscanf ( newInfoString[ 0 ], "%hu,%hu", &info.songs, &info.startSong ) != 2 )
				return false;

			break;
		}

		case INITPLAY:
		{
			if ( std::sscanf ( newInfoString[ 0 ], "%hx,%hx", &info.initAddr, &info.playAddr ) != 2 )
				return false;

			break;
		}

		case FREEPAGES:
		{
			if ( std::sscanf ( newInfoString[ 0 ], "%hhx,%hhx", &info.relocStartPage, &info.relocPages ) != 2 )
				return false;

			if ( ! checkRelocInfo () )
				return false;

			break;
		}

		case FLAGS: // We'll fall thru the next 4 cases for this one.
		case MUSPLAYER:
		{
			if ( mode == FLAGS && newInfoString[ infoStringIndex ][ 0 ] == '*' )
			{
				; // Do nothing - this field is not to be changed.
			}
			else if ( std::atoi ( newInfoString[ infoStringIndex ] ) == 0 )
				info.musPlayer = false;
			else if ( std::atoi ( newInfoString[ infoStringIndex ] ) == 1 )
				info.musPlayer = true;
			else
				return false;

			if ( mode != FLAGS )
				break;

			infoStringIndex++;
			[[ fallthrough ]];
		}

		case PLAYSID:
		{
			if ( mode == FLAGS && newInfoString[ infoStringIndex ][ 0 ] == '*' )
			{
				; // Do nothing - this field is not to be changed.
			}
			else if ( std::atoi ( newInfoString[ infoStringIndex ] ) == 0 )
			{
				if ( ( info.compatibility != SIDTUNE_COMPATIBILITY_C64 ) &&
					 ( info.compatibility != SIDTUNE_COMPATIBILITY_PSID ) )
				{
					return false;
				}
				info.compatibility = SIDTUNE_COMPATIBILITY_C64;
			}
			else if ( std::atoi ( newInfoString[ infoStringIndex ] ) == 1 )
			{
				if ( ( info.compatibility != SIDTUNE_COMPATIBILITY_C64 ) &&
					 ( info.compatibility != SIDTUNE_COMPATIBILITY_PSID ) )
				{
					return false;
				}
				info.compatibility = SIDTUNE_COMPATIBILITY_PSID;
			}
			else
			{
				return false;
			}

			if ( mode != FLAGS )
				break;

			infoStringIndex++;
			[[ fallthrough ]];
		}

		case CLOCK:
		{
			if ( mode == FLAGS && newInfoString[ infoStringIndex ][ 0 ] == '*' )
			{
				; // Do nothing - this field is not to be changed.
			}
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "UNKNOWN" ) )
				info.clockSpeed = PSID_CLOCK_UNKNOWN;
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "PAL" ) )
				info.clockSpeed = PSID_CLOCK_PAL;
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "NTSC" ) )
				info.clockSpeed = PSID_CLOCK_NTSC;
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "ANY" ) || !std::strcmp ( newInfoString[ infoStringIndex ], "EITHER" ) )
				info.clockSpeed = PSID_CLOCK_ANY;
			else
				return false;

			if ( mode != FLAGS )
				break;

			infoStringIndex++;
			[[ fallthrough ]];
		}

		case SIDMODEL:
		{
			if ( mode == FLAGS && newInfoString[ infoStringIndex ][ 0 ] == '*' )
			{
				; // Do nothing - this field is not to be changed.
			}
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "UNKNOWN" ) )
				info.sidModel = PSID_SIDMODEL_UNKNOWN;
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "6581" ) )
				info.sidModel = PSID_SIDMODEL_6581 << psidSidModelShift;
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "8580" ) )
				info.sidModel = PSID_SIDMODEL_8580 << psidSidModelShift;
			else if ( ! std::strcmp ( newInfoString[ infoStringIndex ], "ANY" ) || ! std::strcmp ( newInfoString[ infoStringIndex ], "EITHER" ) )
				info.sidModel = PSID_SIDMODEL_ANY << psidSidModelShift;
			else
				return false;

			break; // The FLAGS directive stops here, too.
		}

		case FIXLOAD:
			// Increase load address by 2 without verification.
			fixLoadAddress ( true, info.initAddr, info.playAddr );
			break;

		default:
			return false;
	}

	return true;
}
//-----------------------------------------------------------------------------
