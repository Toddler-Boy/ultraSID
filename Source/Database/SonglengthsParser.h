#pragma once

#include <JuceHeader.h>

#include <vector>

//-----------------------------------------------------------------------------

// Parses a Songlengths time list ("m:ss m:ss.SSS ...", one entry per subtune)
// into milliseconds; the fraction is optional and scaled by its digit count
// (.5 = .50 = .500)
inline std::vector<uint32_t> parseSonglengthTimes ( const juce::String& list )
{
	const auto	tokens = juce::StringArray::fromTokens ( list, " ", "" );

	std::vector<uint32_t>	timesMs;
	timesMs.reserve ( size_t ( tokens.size () ) );

	for ( const auto& token : tokens )
	{
		const auto	parts = juce::StringArray::fromTokens ( token, ":.", "" );

		auto	ms = uint32_t ( parts[ 0 ].getIntValue () * 60 + parts[ 1 ].getIntValue () ) * 1000u;

		if ( parts.size () > 2 )
			ms += uint32_t ( parts[ 2 ].paddedRight ( '0', 3 ).getIntValue () );

		timesMs.emplace_back ( ms );
	}

	return timesMs;
}
//-----------------------------------------------------------------------------

// Parses a Songlengths.md5-format file: a "[Database]" header line, then per
// tune a "; /MUSICIANS/.../Tune.sid" comment line followed by
// "<md5>=m:ss m:ss.SSS ...". perEntry is called once per tune with the path as
// written (including ".sid"), the times in milliseconds, and the md5. Returns
// false when the file is missing or the header line isn't there.
// Shared by the app (HVSC_database), the scanner and the db builder, for the
// HVSC's Songlengths.md5 as well as the addendum and Songdelays.md5
template <typename PerEntry>
inline bool parseSonglengthsText ( const juce::String& content, PerEntry perEntry )
{
	auto	lines = juce::StringArray::fromLines ( content );
	lines.removeEmptyStrings ();

	if ( lines.isEmpty () || ! lines.getReference ( 0 ).equalsIgnoreCase ( "[Database]" ) )
		return false;

	juce::String	path;

	for ( auto i = 1; i < lines.size (); ++i )
	{
		const auto&	s = lines.getReference ( i );

		// The comment line carries the tune path
		if ( s.startsWithChar ( ';' ) )
		{
			path = s.substring ( 2 ).trim ();
			continue;
		}

		const auto	eq = s.indexOfChar ( '=' );
		if ( eq < 0 || path.isEmpty () )
			continue;

		perEntry ( path, parseSonglengthTimes ( s.substring ( eq + 1 ) ), s.substring ( 0, eq ) );
	}

	return true;
}
//-----------------------------------------------------------------------------

template <typename PerEntry>
inline bool parseSonglengths ( const juce::File& file, PerEntry perEntry )
{
	if ( ! file.existsAsFile () )
		return false;

	return parseSonglengthsText ( file.loadFileAsString (), perEntry );
}
//-----------------------------------------------------------------------------
