#pragma once

#include <JuceHeader.h>

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

//-----------------------------------------------------------------------------

namespace SID
{
	// Splits "tunename,subtune" into its two parts
	[[ nodiscard ]] std::pair<std::string, int> parseTuneName ( const std::string& tuneName );

	[[ nodiscard ]] juce::String convertTimeToString ( int timeMS );
	[[ nodiscard ]] juce::String convertToLongTimeString ( int timeMS );
	[[ nodiscard ]] uint32_t getTuneLength ( std::string_view tuneName, int subTune );

	// Tune-length classification. The +-500 ms mirrors the half-second round-up
	// in convertTimeToString, so the classes agree with the displayed length:
	// anything shown as "0:05" or less is an FX, "0:20" or more is a song.
	constexpr auto	stingerMs = 5 * 1'000 + 500;
	constexpr auto	songMs = 20 * 1'000 - 500;

	[[ nodiscard ]] inline bool isFX ( const int lengthMs ) 		{	return lengthMs < stingerMs;	}
	[[ nodiscard ]] inline bool isStinger ( const int lengthMs )	{	return lengthMs < songMs;		}
	[[ nodiscard ]] inline bool isSong ( const int lengthMs )		{	return lengthMs >= songMs;		}

	// ( render length + fade, fade length, LUFS, filter used, silent-intro skip ), all lengths in ms
	[[ nodiscard ]] std::tuple<uint32_t, uint32_t, float, bool, uint32_t> getRenderInfo ( const std::string& tuneName, const int subtune );
}
//-----------------------------------------------------------------------------
