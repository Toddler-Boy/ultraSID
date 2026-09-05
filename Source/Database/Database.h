#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "libSidplayEZ/src/EZ/override-selector.h"
#include "libSidplayEZ/src/sidtune/PSID.h"

#include "Database/uSIDFormat.h"

//-----------------------------------------------------------------------------

constexpr auto	maxTunesArray = 5;	// Hand tuned for maximum memory efficiency

// Pre-compiled 256-byte Look-Up Table mapping characters to lowercase
// and Extended ASCII (Windows-1252/ISO-8859-1) characters to their plain ASCII base equivalents.
constexpr uint8_t sortingLut[ 256 ] =
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	// 0x20 - 0x2F (Space, Punctuation)
	' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', 0x7f,
	// 0x80 - 0x8F (Extended ASCII Windows-1252 extras)
	0x80, 0x81, 0x82, 'f', 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 's', 0x8b, 'o', 0x8d, 'z', 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 'o', 0x9d, 'z', 'y',
	0xa0, '!', 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
	'd', 'n', 'o', 'o', 'o', 'o', 'o', 'x', 'o', 'u', 'u', 'u', 'u', 'y', 't', 's',
	'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
	'o', 'n', 'o', 'o', 'o', 'o', 'o', 0xf7, 'o', 'u', 'u', 'u', 'u', 'y', 't', 'y'
};

class Database
{
public:
	// this
	int load ( const juce::MemoryBlock& mb );
	[[ nodiscard ]] int getVersion () const;

	void applyOverrides ( const libsidplayEZ::OverrideSelector::overrideMap& overMap );

	[[ nodiscard ]] float getSongLoudness ( const std::string& filename, unsigned int songNo ) const;
	[[ nodiscard ]] float getSongMidLoudness ( const std::string& filename, unsigned int songNo ) const;
	[[ nodiscard ]] bool getSongFilterUsed ( const std::string& filename, unsigned int songNo ) const;

	// Unknown tunes count as digi-less
	[[ nodiscard ]] bool getSongDigiUsed ( const std::string& filename, unsigned int songNo ) const;

	// A one-shot song ends for good instead of looping; unknown tunes count as looping
	[[ nodiscard ]] bool getSongIsOneShot ( const std::string& filename, unsigned int songNo ) const;

	struct entry
	{
		// Views into database-owned storage (arenas for HVSC, per-entry
		// backing for user tunes); not NUL-terminated
		std::string_view	file;
		std::string_view	name;
		std::string_view	author;
		std::string_view	release;

		// The folded search line "lowerFile NUL lowerName NUL lowerAuthor NUL
		// lowerRelease"; the lower* views are subviews of it
		std::string_view	search;
		std::string_view	lowerFile;
		std::string_view	lowerName;
		std::string_view	lowerRelease;

		// Interleaved word pairs per subtune, layout in Database/uSIDFormat.h
		static constexpr int	arraySlots = maxTunesArray * usid::wordsPerSubtune;

		#pragma pack(push, 2)
		union TuneProperties
		{
			int16_t*	propsPtr;
			int16_t		propsArr[ arraySlots ];
		};
		#pragma pack(pop)

		TuneProperties	tuneProperties;

		uint16_t	numTunes;
		uint16_t	flags;			// The tune's PSID header flags word, verbatim, see PSID.h
		uint16_t	startTune;

		bool	userTune = false;

		void init ( const int16_t* const initData, int16_t* dstToUse )
		{
			if ( numTunes > maxTunesArray )
			{
				tuneProperties.propsPtr = dstToUse;

				// copy the first pair as to not waste any space
				tuneProperties.propsArr[ arraySlots - 2 ] = initData[ 0 ];
				tuneProperties.propsArr[ arraySlots - 1 ] = initData[ 1 ];

				std::copy ( initData + usid::wordsPerSubtune, initData + numTunes * usid::wordsPerSubtune, tuneProperties.propsPtr );
			}
			else
			{
				std::copy ( initData, initData + numTunes * usid::wordsPerSubtune, tuneProperties.propsArr );
			}
		}

		void initUser ( const int16_t defaultProp )
		{
			if ( numTunes > maxTunesArray )
				tuneProperties.propsPtr = nullptr;
			else
				for ( auto i = 0; i < maxTunesArray; ++i )
				{
					tuneProperties.propsArr[ i * usid::wordsPerSubtune ] = defaultProp;
					tuneProperties.propsArr[ i * usid::wordsPerSubtune + 1 ] = 0;
				}
		}

		[[ nodiscard ]] int16_t getProperties ( const int songNo ) const;
		[[ nodiscard ]] int16_t getMidWord ( const int songNo ) const;

		[[ nodiscard ]] bool hasFilter ( const int songNo ) const		{	return usid::hasFilter ( getProperties ( songNo ) );	}
		[[ nodiscard ]] bool hasDigi ( const int songNo ) const			{	return usid::hasDigi ( getProperties ( songNo ) );	}
		[[ nodiscard ]] bool hasOneShot ( const int songNo ) const		{	return usid::hasOneShot ( getProperties ( songNo ) );	}
		[[ nodiscard ]] float getLoudness ( const int songNo ) const;
		[[ nodiscard ]] float getMidLoudness ( const int songNo ) const;

		// A tune that declares both clocks counts as NTSC
		[[ nodiscard ]] bool isNTSC () const	{	return flags & libsidplayfp::PSID_CLOCK_NTSC;	}

		[[ nodiscard ]] bool hasAnyFlag ( const int flag ) const;
		[[ nodiscard ]] bool hasAnyFilter () const			{	return hasAnyFlag ( 1 );		}
		[[ nodiscard ]] bool hasAnyDigi () const			{	return hasAnyFlag ( 2 );		}
		[[ nodiscard ]] bool hasAnyOneShot () const			{	return hasAnyFlag ( 4 );		}
	};

	[[ nodiscard ]] std::vector<const Database::entry*> getAllEntries ();
	[[ nodiscard ]] const Database::entry* findEntry ( const std::string& hvscPath ) const;

	// The entry to read song properties from; null when the song number is 0
	// (unknown) or the tune is in neither database
	[[ nodiscard ]] const Database::entry* entryForSong ( const std::string& filename, unsigned int songNo ) const;

protected:
	std::unordered_map<std::string_view, entry>	db;	// keys alias entry.file

	std::vector<int16_t>	allSubtuneProperties;

	// The entry views' storage: originals in one arena, folded search lines
	// in the other. Sized exactly up front, the views never move
	std::vector<char>	stringArena;
	std::vector<char>	searchArena;

private:
	int		hvscVersion = 0;
};
//-----------------------------------------------------------------------------

class UserDatabase final : public Database
{
public:
	// this
	void scanUserTunes ();

	void addUserTune ( const juce::File& file );
	void removeUserTune ( const juce::File& file );

private:
	[[ nodiscard ]] static std::string getKey ( const juce::File& file );

	void resolveNames ();

	// Storage behind one user entry's views: file aliases the map key,
	// author/release the originals, name/search the texts resolveNames rebuilds
	struct Backing
	{
		std::string	name;		// header original, entries may show a substitute
		std::string	author;
		std::string	release;
		std::string	texts;		// shown name + the folded search line
	};
	std::unordered_map<std::string, Backing>	backing;
};
//-----------------------------------------------------------------------------

namespace db
{
	// Looks up filename in the HVSC database first, then the user database
	[[ nodiscard ]] const Database::entry* findDatabaseEntry ( const std::string& filename );
}
//-----------------------------------------------------------------------------
