#include <JuceHeader.h>

#include <fmt/format.h>

#include "TuneInfo.h"

#include "Audio/PerceivedLoudness.h"
#include "Config/FilePaths.h"
#include "Config/Preferences.h"
#include "Database/Database.h"
#include "Database/HVSCDatabase.h"
#include "Database/UserLoudness.h"

//-----------------------------------------------------------------------------

std::pair<std::string, int> SID::parseTuneName ( const std::string& tuneName )
{
	const auto	commaOffset = tuneName.find_last_of ( ',' );
	auto	subTune = 0;

	// If this entry uses a subtune, remember it
	if ( commaOffset != std::string::npos )
		subTune = std::atoi ( tuneName.substr ( commaOffset + 1 ).c_str () );

	return { tuneName.substr ( 0, commaOffset ), subTune };
}
//-----------------------------------------------------------------------------

juce::String SID::convertTimeToString ( int timeMS )
{
	if ( timeMS < 0 )
		return {};

	// Round up by half a second
	timeMS += 500;

	// Convert millisecond counter to human readable time
	const auto	hours = timeMS / ( 1000 * 60 * 60 );		timeMS -= hours * 1000 * 60 * 60;
	const auto	min = timeMS / ( 1000 * 60 );				timeMS -= min * 1000 * 60;
	const auto	sec = timeMS / 1000;

	if ( hours )	return fmt::format ( "{}:{:02}:{:02}", hours, min, sec );

	return fmt::format ( "{}:{:02}", min, sec );
}
//-----------------------------------------------------------------------------

juce::String SID::convertToLongTimeString ( int timeMS )
{
	if ( timeMS < 0 )
		return {};

	// Round up by half a second
	timeMS += 500;

	// Convert millisecond counter to human readable time
	const auto	days = timeMS / ( 1000 * 60 * 60 * 24 );	timeMS -= days * 1000 * 60 * 60 * 24;
	const auto	hours = timeMS / ( 1000 * 60 * 60 );		timeMS -= hours * 1000 * 60 * 60;
	const auto	min = timeMS / ( 1000 * 60 );				timeMS -= min * 1000 * 60;
	const auto	sec = timeMS / 1000;

	auto	ret = juce::String ();

	if ( days )		ret += juce::String ( days ) + " days ";
	if ( hours )	ret += juce::String ( hours ) + " hr ";
	if ( min )		ret += juce::String ( min ) + " min ";
	if ( ! hours )	ret += juce::String ( sec ) + " sec";

	return ret.trim ();
}
//-----------------------------------------------------------------------------

uint32_t SID::getTuneLength ( const std::string_view tuneName, int subTune )
{
	const juce::SharedResourcePointer<HVSC_database>	hvscDB;

	if ( const auto	lengthMs = hvscDB->getLengthMs ( tuneName, subTune ) )
		return lengthMs;

	const juce::SharedResourcePointer<Preferences>	settings;

	return settings->getClamped ( "songs/unknown" ) * 60u * 1000u;
}
//-----------------------------------------------------------------------------

std::tuple<uint32_t, uint32_t, float, bool, uint32_t> SID::getRenderInfo ( const std::string& tuneName, const int subtune )
{
	const juce::SharedResourcePointer<HVSC_database>	hvscDB;
	const juce::SharedResourcePointer<Database>			database;

	// The rating composed from the two stored measurements; an unknown tune
	// stays -96 and gets measured live instead
	auto	lufs = float ( PerceivedLoudness::composeRating ( database->getSongLoudness ( tuneName, subtune ),
															  database->getSongMidLoudness ( tuneName, subtune ) ) );

	// User tunes keep their live measurements in the cache
	if ( lufs <= -95.9f && tuneName.starts_with ( filepaths::userMarker ) )
	{
		const juce::SharedResourcePointer<UserLoudness>	userLoudness;

		const auto [ raw, mid ] = userLoudness->get ( UserLoudness::keyFor ( tuneName ), unsigned ( subtune ) );
		lufs = float ( PerceivedLoudness::composeRating ( raw, mid ) );
	}
	auto		len = SID::getTuneLength ( tuneName, subtune );
	const auto	filterUsed = database->getSongFilterUsed ( tuneName, subtune );

	// The silent intro gets skipped by pre-rendering; the songlength ignores it too
	const auto	startMs = hvscDB->getStartMs ( tuneName, subtune );

	// A one-shot song ends for good: extending it just renders silence, and a
	// fade-out would fade what has already stopped, so it plays its real length
	if ( database->getSongIsOneShot ( tuneName, subtune ) )
		return { len, 0, lufs, filterUsed, startMs };

	const juce::SharedResourcePointer<Preferences>	settings;

	// Extend song-render-length considering minimum-length, repetition count, and
	// fade-out. Clamped fetches: hand-edited yml values must never reach the
	// render length/allocation math unbounded (negative wraps via * 1000u)
	{
		auto		songMinimum = settings->getClamped ( "songs/minimum" ) * 1000u;
		const auto	songLoops = settings->getClamped ( "songs/max-loops" ) * len;

		if ( songLoops )
			songMinimum = std::min ( songLoops, songMinimum );

		len = std::max ( len, songMinimum );
	}
	const auto	songFade = settings->getClamped ( "songs/fade-out" ) * 1000u;

	return { len + songFade, songFade, lufs, filterUsed, startMs };
}
//-----------------------------------------------------------------------------
