#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

//
// Peak-level tracker feeding the UI meters. The audio thread reports buffer
// peaks via trackBuffer(); the UI polls getLevel(), which decays the held peak
// at a fixed dB/s rate. Self-contained on purpose, keeping the audio layer
// free of framework code.
//
class LevelTracker final
{
public:
	LevelTracker ( const float decayDbPerSecond = 30.0f )
		: decayPerSecond ( decayDbPerSecond )
	{
	}
	//-----------------------------------------------------------------------------

	void trackBuffer ( const float* buffer, const int numSamples )
	{
		auto	peak = 0.0f;
		for ( auto i = 0; i < numSamples; ++i )
			peak = std::max ( peak, std::fabs ( buffer[ i ] ) );

		if ( peak >= 1.0f )
			clipTime.store ( now (), std::memory_order_relaxed );

		const auto	db = 20.0f * std::log10 ( std::max ( peak, 1.0e-5f ) );	// floored at -100 dB
		if ( db >= getLevel () )
		{
			peakDb.store ( db, std::memory_order_relaxed );
			peakTime.store ( now (), std::memory_order_relaxed );
		}

		if ( db >= getHold () )
		{
			holdDb.store ( db, std::memory_order_relaxed );
			holdTime.store ( now (), std::memory_order_relaxed );
		}
	}
	//-----------------------------------------------------------------------------

	// Current level in dB (<= 0), decayed since the last held peak
	[[ nodiscard ]] float getLevel () const
	{
		const auto	elapsed = float ( now () - peakTime.load ( std::memory_order_relaxed ) );

		return std::max ( peakDb.load ( std::memory_order_relaxed ) - decayPerSecond * elapsed, -100.0f );
	}
	//-----------------------------------------------------------------------------

	// Peak-hold: the maximum level of the last holdSeconds, -100 once expired
	[[ nodiscard ]] float getHold () const
	{
		if ( now () - holdTime.load ( std::memory_order_relaxed ) > holdSeconds )
			return -100.0f;

		return holdDb.load ( std::memory_order_relaxed );
	}
	//-----------------------------------------------------------------------------

	// Clipping clears itself after clipSeconds (or explicitly via clearClip)
	[[ nodiscard ]] bool getClip () const	{	return now () - clipTime.load ( std::memory_order_relaxed ) < clipSeconds;	}
	void clearClip ()						{	clipTime.store ( 0.0, std::memory_order_relaxed );	}
	//-----------------------------------------------------------------------------

private:
	[[ nodiscard ]] static double now ()
	{
		return std::chrono::duration<double> ( std::chrono::steady_clock::now ().time_since_epoch () ).count ();
	}

	static constexpr auto	holdSeconds = 2.0;
	static constexpr auto	clipSeconds = 2.5;

	float	decayPerSecond = 30.0f;

	std::atomic<float>	peakDb { -100.0f };
	std::atomic<double>	peakTime { 0.0 };
	std::atomic<float>	holdDb { -100.0f };
	std::atomic<double>	holdTime { 0.0 };
	std::atomic<double>	clipTime { 0.0 };
};
//-----------------------------------------------------------------------------
