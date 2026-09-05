#pragma once

#include <cmath>
#include <numbers>
#include <span>

//-----------------------------------------------------------------------------

namespace fft::helpers
{

inline void window_BlackmanHarris ( std::span<float> data )
{
	const auto	n = int ( data.size () );

	// Pre-calculate blackman-harris window
	for ( auto i = 0; i < n; ++i )
	{
		const auto	phase = float ( std::numbers::pi ) * 2.0f * ( float ( i ) / float ( n - 1 ) );
		data[ i ] = 0.35875f - 0.48829f * std::cos ( phase ) + 0.14128f * std::cos ( phase * 2.0f ) - 0.01168f * std::cos ( phase * 3.0f );
	}
}

inline void window_Hann ( std::span<float> data )
{
	const auto	n = int ( data.size () );

	// Pre-calculate Hann window
	for ( auto i = 0; i < n; ++i )
		data[ i ] = 0.5f * ( 1.0f - std::cos ( 2.0f * float ( std::numbers::pi ) * float ( i ) / float ( n - 1 ) ) );
}

inline void applyWindow ( std::span<float> dst, std::span<float> src, std::span<float> window )
{
	const auto	n = int ( window.size () );

	for ( auto i = 0; i < n; ++i )
		dst[ i ] = src[ i ] * window[ i ];
}

inline void applyWindow ( std::span<float> dst, std::span<float> window )
{
	const auto	n = int ( window.size () );

	for ( auto i = 0; i < n; ++i )
		dst[ i ] *= window[ i ];
}

constexpr auto linearToDecibel = [] ( float gain ) -> float { return ( std::log10 ( gain ) ) * 20.0f; };
constexpr auto decibleToLinear = [] ( float db ) -> float {	return std::pow ( 10.0f, db * 0.05f ); };

}
//-----------------------------------------------------------------------------

namespace UI::fft
{
	constexpr auto	numHistory = 16;
	constexpr auto	glow = 8.0f;

	constexpr auto	fftFreqStart = 12.0f;
	constexpr auto	fftFreqStop = 12'000.0f;

	inline const float	freqStartLog = std::log10 ( fftFreqStart );
	inline const float	freqRangeLog = std::log10 ( fftFreqStop ) - freqStartLog;

	[[ nodiscard ]] inline float freqToNormalized ( const float freq )
	{
		return ( std::log10 ( freq ) - freqStartLog ) / freqRangeLog;
	}

	[[ nodiscard ]] inline float pow2 ( const float a ) { return a * a; }
}
//-----------------------------------------------------------------------------
