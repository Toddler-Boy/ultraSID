#pragma once

#include <JuceHeader.h>

#include <string>
#include <vector>

#include "libSidplayEZ/src/sidtune/PSID.h"

// Leave TITLE, AUTHOR, RELEASED in exact order as enumerated below.
const static std::vector<std::string>	keywords =
{
	"TITLE", "AUTHOR", "RELEASED", "SPEED", "SONGS",
	"CREDITS", "DELETE", "MOVE", "REPLACE", "MKDIR", "FIXLOAD",
	"INITPLAY", "MUSPLAYER", "PLAYSID", "CLOCK", "SIDMODEL",
	"FREEPAGES", "FLAGS", "COPYRIGHT",
};
//-----------------------------------------------------------------------------

enum mode_type
{
	TITLE = 0, AUTHOR = 1, RELEASED = 2, SPEED, SONGS,
	CREDITS, DELETEMODE, MOVE, REPLACE, MKDIRMODE, FIXLOAD,
	INITPLAY, MUSPLAYER, PLAYSID, CLOCK, SIDMODEL,
	FREEPAGES, FLAGS, COPYRIGHT,
	NO_MODE
};
//-----------------------------------------------------------------------------

constexpr auto	maxSidInfoLen = 32;  // not including terminator

constexpr auto	classMaxSongs = 256u;		// also file format limit
constexpr auto	infoStringNum = 5u;			// maximum
constexpr auto	infoStringLen = 80u + 1u;	// 80 characters plus terminating zero

constexpr auto	maxSidtuneFileLen = 65536u + 2u + 0x7Cu;	// C64KB + LOAD + PSID

constexpr auto	SIDTUNE_SPEED_VBI = 0;	  // Vertical-Blanking-Interrupt
constexpr auto	SIDTUNE_SPEED_CIA_1A = 60;  // CIA 1 Timer A

constexpr auto	SIDTUNE_COMPATIBILITY_C64 = 0x00; // File is C64 compatible
constexpr auto	SIDTUNE_COMPATIBILITY_PSID = 0x01; // File is PSID specific
constexpr auto	SIDTUNE_COMPATIBILITY_R64 = 0x02; // File is Real C64 only
constexpr auto	SIDTUNE_COMPATIBILITY_BASIC = 0x03; // File requires C64 Basic

//-----------------------------------------------------------------------------

struct updater_sidTuneInfo
{
	// Consider the following fields as read-only, because the sidTune class
	// does not provide an implementation of: bool setInfo(sidTuneInfo&).
	// Currently, the only way to get the class to accept values which
	// are written to these fields is by creating a derived class.
	//
	uint16_t	version;		// added for v3
	const char*	formatString;	// the name of the identified file format
	const char*	speedString;	// describing the speed a song is running at
	uint16_t	loadAddr;
	uint16_t	initAddr;
	uint16_t	playAddr;
	uint16_t	startSong;
	uint16_t	songs;

	//
	// Available after song initialization.
	//
	uint16_t irqAddr;				// if (playAddr == 0), interrupt handler has been
	// installed and starts calling the C64 player
	// at this address
	uint16_t currentSong;			// the one that has been initialized
	uint8_t songSpeed;			// intended speed, see top
	uint8_t clockSpeed;			// PSID_CLOCK_* bits, in their header positions
	bool musPlayer;				// whether Sidplayer routine has been installed
	int	 compatibility;			// compatibility requirements
	uint16_t sidModel;				// PSID_SIDMODEL* bits for all three chips, in their header positions
	uint16_t reserved;				// SID address of 2nd sid; added for v3
	bool fixLoad;				// whether load address might be duplicate
	uint16_t lengthInSeconds;		// --- not yet supported ---

	uint8_t relocStartPage;		// First available page for relocation.
	uint8_t relocPages;			// Number of pages available for relocation.

	//
	// Song title, credits, ...
	//
	uint8_t numberOfInfoStrings;	// the number of available text info lines
	char* infoString[ infoStringNum ];
	char* nameString;			// name, author and copyright strings
	char* authorString;			// are duplicates of infoString[?]
	char* copyrightString;
	//
	uint32_t dataFileLen;			// length of single-file sidtune file
	uint32_t c64dataLen;			// length of raw C64 data without load address
	//
	const char* statusString;	// error/status message of last operation
};
//-----------------------------------------------------------------------------

class updater_sidTune final
{
public:
	updater_sidTune ( const juce::MemoryBlock& data );

	//-----------------------------------------------------------------------------

	[[ nodiscard ]] bool getStatus ()	{	return status;	}

	[[ nodiscard ]] bool writeToSidTune ( char newInfoString[][ maxSidInfoLen + 1 ], mode_type mode );

	// The rewritten PSID file; empty on a bad object or a write problem
	[[ nodiscard ]] juce::MemoryBlock savePSIDData ();

	// This function can be used to remove a duplicate C64 load address in
	// the C64 data (example: FE 0F 00 10 4C ...). A duplicate load address
	// of offset 0x02 is indicated by the ``fixLoad'' flag in the sidTuneInfo
	// structure.
	//
	// The ``force'' flag here can be used to remove the first load address
	// and set new INIT/PLAY addresses regardless of whether a duplicate
	// load address has been detected and indicated by ``fixLoad''.
	// For instance, some position independent sidtunes contain a load address
	// of 0xE000, but are loaded to 0x0FFE and call the player code at 0x1000.
	//
	// Don't forget to save the sidtune file.
	void fixLoadAddress ( bool force = false, uint16_t initAddr = 0, uint16_t playAddr = 0 );

protected:
	bool		status = false;
	updater_sidTuneInfo	info;

	uint8_t	songSpeed[ classMaxSongs ];
	uint8_t clockSpeed[ classMaxSongs ];
	uint16_t songLength[ classMaxSongs ];   // song lengths in seconds

	// holds text info from the format headers etc.
	char infoString[ infoStringNum ][ infoStringLen ] = {};

	std::vector<uint8_t>	cachedData;

	std::vector<uint8_t>	fileBuf;
	uint32_t				fileOffset = 0;	// for files with header: offset to real data

	// Convert 32-bit PSID-style speed word to internal tables.
	void convertOldStyleSpeedToTables ( uint32_t oldStyleSpeed, int clock = libsidplayfp::PSID_CLOCK_PAL );

	[[ nodiscard ]] bool checkRealC64Info ( uint32_t speed );
	[[ nodiscard ]] bool checkCompatibility ( void );
	[[ nodiscard ]] bool checkRelocInfo ( void );

	[[ nodiscard ]] uint32_t loadData ( const juce::MemoryBlock& data );

	// Data caching.
	bool cacheRawData ( const uint8_t* sourceBuffer, uint32_t sourceBufLen );

	// Support for PSID format
	[[ nodiscard ]] bool PSID_fileSupport ( const void* buffer, uint32_t bufLen );
	[[ nodiscard ]] bool PSID_fileSupportSave ( juce::OutputStream& toFile, const uint8_t* dataBuffer );

private:
	// Cache the data of a single-file
	void acceptSidTune ( const uint8_t* dataFileBuf, uint32_t dataLen );
};
//-----------------------------------------------------------------------------
