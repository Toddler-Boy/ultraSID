#pragma once

#include <cstdint>

//-----------------------------------------------------------------------------

// The ultraSID.db container format, shared by the writer (sid_scanner's
// DatabaseBuilder) and the reader (Database::load) so the two cannot drift.
// The database ships inside the app, so the format carries no version field:
// both sides are always built from this header.
//
// Layout: magic "uSID", u8 HVSC version, u32 payload length, raw payload.
// (The payload is stored raw so the file is hex-inspectable and bit-identical
// across rebuilds; Data.pak compresses it)
// Payload: u32 entry count, then per entry: pascal-strings file/name/author/
// release, u16 flags, u16 startTune, u16 numTunes, numTunes word pairs

namespace usid
{
	inline constexpr char		magic[ 4 ] = { 'u', 'S', 'I', 'D' };
	inline constexpr int		headerSize = 9;

	// Two int16 words per subtune, dB values stored * 10 with the low 4 bits
	// as feature space. First word: bits 15-4 = integrated loudness (signed,
	// all-zero = unmeasured), bit 3 = unused, bit 2 = one-shot (the tune ends
	// for good instead of looping), bit 1 = digi, bit 0 = filter. Second word:
	// bits 15-4 = midband loudness, bits 3-0 = spare feature bits.
	// (Delayed starts are not in here, the shipped Songdelays.md5 is their
	// single source of truth)
	inline constexpr int		wordsPerSubtune = 2;

	[[ nodiscard ]] inline constexpr int16_t packProperties ( const float loudness, const bool filter, const bool digi, const bool oneShot )
	{
		return int16_t ( ( int ( loudness * 10.0f ) << 4 ) | ( oneShot ? 4 : 0 ) | ( digi ? 2 : 0 ) | ( filter ? 1 : 0 ) );
	}

	[[ nodiscard ]] inline constexpr int16_t packMidLoudness ( const float midLoudness )
	{
		return int16_t ( int ( midLoudness * 10.0f ) << 4 );
	}

	[[ nodiscard ]] inline constexpr bool hasFilter ( const int16_t props )		{	return props & 1;	}
	[[ nodiscard ]] inline constexpr bool hasDigi ( const int16_t props )		{	return props & 2;	}
	[[ nodiscard ]] inline constexpr bool hasOneShot ( const int16_t props )	{	return props & 4;	}

	// Unmeasured reads as -96 dB
	[[ nodiscard ]] inline constexpr float getLoudness ( const int16_t props )
	{
		const auto	loudness = props >> 4;

		return loudness ? float ( loudness ) * 0.1f : -96.0f;
	}

	[[ nodiscard ]] inline constexpr float getMidLoudness ( const int16_t word )
	{
		return getLoudness ( word );
	}
}
//-----------------------------------------------------------------------------
