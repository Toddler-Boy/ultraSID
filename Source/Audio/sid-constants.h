#pragma once

//-----------------------------------------------------------------------------

namespace SID
{
	constexpr auto	numVoices = 3;

	// Each SID voice occupies 7 registers ($D400-$D406 for voice 1, etc.)
	constexpr auto	REGISTER_VOICE_DELTA = 7;

	// Hz per SID frequency-register LSB: output freq = register * clock / 2^24
	// (985248 Hz = PAL phi2 clock, 1022730 Hz = NTSC)
	constexpr auto	PAL_CLOCK	=  985248.0f / 16777216.0f;
	constexpr auto	NTSC_CLOCK	= 1022730.0f / 16777216.0f;
}
//-----------------------------------------------------------------------------
