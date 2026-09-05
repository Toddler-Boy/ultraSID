#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "FX_Helpers.h"

// Stereo multi-band EQ. Each band is an RBJ biquad: low shelf, peaking,
// high shelf, low-pass, or high-pass (12 dB/oct cutoffs; their gain acts
// as plain output gain). Band count is adjustable at setup time (not on the
// audio thread); cost is linear in the active band count.
// The block is interleaved once into an L1-resident double scratch buffer,
// all bands run over the interleaved pairs (one 2-lane load/store per
// sample on NEON/SSE2), then de-interleaved back to float L/R.
// Gain changes are smoothed (one-pole ramp on the dB value).
// Default: 3 bands as a loudness curve
// (+4 dB low shelf @ 150 Hz, flat peak @ 700 Hz, +3 dB high shelf @ 4 kHz).
class MultiBandEQ final
{
public:
	enum type { lowShelf, peak, highShelf, lowPass, highPass };

	//-----------------------------------------------------------------------------

	explicit MultiBandEQ ( int numBands = 3, double sampleRate = 44100.0 )
	{
		setSampleRate ( sampleRate );
		setNumBands ( numBands );

		// Loudness curve default: +4 dB low shelf @ 150 Hz, flat peak @ 700 Hz, +3 dB high shelf @ 4 kHz
		setBand ( 0, lowShelf, 150.0, 4.0, 0.707 );
		setBand ( 1, peak, 700.0, 0.0, 0.707 );
		setBand ( 2, highShelf, 4000.0, 3.0, 0.707 );
	}
	//-----------------------------------------------------------------------------

	void setSampleRate ( double sampleRate )
	{
		fs = sampleRate;

		smoothCoeff = 1.0 - std::exp ( -1.0 / ( smoothingMs * 0.001 * fs ) );

		for ( auto i = 0; i < static_cast<int> ( bands.size () ); ++i )
			updateCoeffs ( i );
	}
	//-----------------------------------------------------------------------------

	// Not audio-thread safe (allocates). Configure before processing.
	// Layout: band 0 = low shelf @ 100 Hz, last = high shelf @ 8 kHz,
	// middles = flat peaks log-spaced between them. With the default
	// 3 bands the shelves get loudness gains (+4 / +3 dB).
	void setNumBands ( int n )
	{
		bands.assign ( n, Biquad {} );

		const auto	loudness = ( n == 3 );

		for ( auto i = 0; i < n; ++i )
		{
			auto&	b = bands[ i ];
			if ( i == 0 )
			{
				b.type = lowShelf;  b.freq = 100.0;  b.Q = 0.9;
				b.targetDb = b.currentDb = loudness ? 4.0 : 0.0;
			}
			else if ( i == n - 1 )
			{
				b.type = highShelf; b.freq = 8000.0; b.Q = 0.9;
				b.targetDb = b.currentDb = loudness ? 3.0 : 0.0;
			}
			else
			{
				b.type = peak; b.Q = 0.707;
				b.freq = 100.0 * std::pow ( 80.0, (double)i / ( n - 1 ) );
			}
			updateCoeffs ( i );
		}
	}
	//-----------------------------------------------------------------------------

	[[ nodiscard ]] int getNumBands () const { return static_cast<int>( bands.size () ); }

	// Ramp time for gain changes (default 30 ms)
	void setSmoothingTime ( double ms )
	{
		smoothingMs = ms;
		smoothCoeff = 1.0 - std::exp ( -1.0 / ( smoothingMs * 0.001 * fs ) );
	}
	//-----------------------------------------------------------------------------

	// Full band configuration, applied immediately. setGain is the ramped runtime path
	void setBand ( int band, type type, double freqHz, double gainDb = 0.0, double Q = 0.707 )
	{
		auto&	b = bands[ band ];
		b.type = type;
		b.freq = freqHz;
		b.Q = Q;
		b.targetDb = b.currentDb = gainDb;
		updateCoeffs ( band );
	}
	//-----------------------------------------------------------------------------

	// Call this from the slider. Ramps to the new value.
	void setGain ( int band, double gainDb )
	{
		bands[ band ].targetDb = gainDb;
	}
	//-----------------------------------------------------------------------------

	// Jump without ramp (e.g. preset load before playback starts)
	void setGainImmediate ( int band, double gainDb )
	{
		bands[ band ].targetDb = bands[ band ].currentDb = gainDb;
		updateCoeffs ( band );
	}
	//-----------------------------------------------------------------------------

	[[ nodiscard ]] double getGain ( int band ) const { return bands[ band ].targetDb; }
	[[ nodiscard ]] double getFreq ( int band ) const { return bands[ band ].freq; }
	[[ nodiscard ]] double getQ ( int band ) const { return bands[ band ].Q; }
	[[ nodiscard ]] type getType ( int band ) const { return bands[ band ].type; }

	//-----------------------------------------------------------------------------

	// Combined magnitude response of all bands at freqHz, in dB: the
	// curve the user actually hears (bands run in series, so their
	// magnitudes multiply). Uses the target gains (what the sliders
	// show), not the smoothed in-flight values, so the displayed curve
	// reacts instantly. Safe to call from a UI thread; does not touch
	// filter state.
	[[ nodiscard ]] double getMagnitudeDb ( double freqHz ) const
	{
		const auto	w = 2.0 * std::numbers::pi * freqHz / fs;
		const auto	cw = std::cos ( w );
		const auto	c2w = std::cos ( 2.0 * w );

		auto	mag2 = 1.0;
		for ( const auto& bd : bands )
		{
			Coeffs c;
			computeCoeffs ( bd.type, bd.freq, bd.targetDb, bd.Q, c );

			// |H(e^jw)|^2 for b0 + b1 z^-1 + b2 z^-2 over 1 + a1 z^-1 + a2 z^-2
			const auto	num =	c.b0 * c.b0 + c.b1 * c.b1 + c.b2 * c.b2
								+ 2.0 * ( c.b0 * c.b1 + c.b1 * c.b2 ) * cw
								+ 2.0 * c.b0 * c.b2 * c2w;

			const auto	den =	1.0 + c.a1 * c.a1 + c.a2 * c.a2
								+ 2.0 * ( c.a1 + c.a1 * c.a2 ) * cw
								+ 2.0 * c.a2 * c2w;

			mag2 *= num / den;
		}

		return 10.0 * std::log10 ( mag2 );
	}
	//-----------------------------------------------------------------------------

	void process ( float* __restrict left, float* __restrict right, int numSamples )
	{
		alignas( 16 ) double	scratch[ 2 * kChunk ];

		for ( auto off = 0; off < numSamples; off += kChunk )
		{
			auto* __restrict	L = left + off;
			auto* __restrict	R = right + off;

			const auto	n = std::min ( kChunk, numSamples - off );

			for ( auto s = 0; s < n; ++s )
			{
				scratch[ 2 * s ] = L[ s ];
				scratch[ 2 * s + 1 ] = R[ s ];
			}

			const auto	bndCnt = static_cast<int>( bands.size () );
			for ( auto i = 0; i < bndCnt; ++i )
				processBand ( i, scratch, n );

			for ( auto s = 0; s < n; ++s )
			{
				L[ s ] = static_cast<float> ( scratch[ 2 * s ] );
				R[ s ] = static_cast<float> ( scratch[ 2 * s + 1 ] );
			}
		}
	}
	//-----------------------------------------------------------------------------

	void reset ()
	{
		for ( auto& b : bands )
			b.z1[ 0 ] = b.z1[ 1 ] = b.z2[ 0 ] = b.z2[ 1 ] = 0.0;
	}
	//-----------------------------------------------------------------------------

private:
	static constexpr int kChunk = 256; // 4 KB scratch, stays in L1

	struct Biquad
	{
		type type = peak;
		double freq = 1000.0, Q = 0.707;
		double targetDb = 0.0, currentDb = 0.0;
		double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
		double z1[ 2 ] = { 0, 0 }, z2[ 2 ] = { 0, 0 }; // [L, R], transposed DF2 state
	};

	// One band over interleaved [L,R] double pairs, via the shared
	// 2-lane double shim from FX_Helpers.h
	void processBand ( int band, double* __restrict buf, int n )
	{
		auto&	b = bands[ band ];
		auto	z1 = simd::v2_load ( b.z1 );
		auto	z2 = simd::v2_load ( b.z2 );

		if ( std::fabs ( b.targetDb - b.currentDb ) > 1.0e-4 ) [[ unlikely ]] // ramping
		{
			for ( auto s = 0; s < n; ++s )
			{
				b.currentDb += smoothCoeff * ( b.targetDb - b.currentDb );
				updateCoeffs ( band );
				const auto	b0 = simd::v2_dup ( b.b0 ), b1 = simd::v2_dup ( b.b1 ), b2 = simd::v2_dup ( b.b2 );
				const auto	a1 = simd::v2_dup ( b.a1 ), a2 = simd::v2_dup ( b.a2 );

				const auto	x = simd::v2_load ( buf + 2 * s );
				const auto	y = simd::v2_fma ( z1, b0, x );                       // b0*x + z1
				z1 = simd::v2_fms ( simd::v2_fma ( z2, b1, x ), a1, y );          // b1*x + z2 - a1*y
				z2 = simd::v2_fms ( simd::v2_mul ( b2, x ), a2, y );              // b2*x - a2*y
				simd::v2_store ( buf + 2 * s, y );
			}
		}
		else
		{
			const auto	b0 = simd::v2_dup ( b.b0 );
			const auto	b1 = simd::v2_dup ( b.b1 );
			const auto	b2 = simd::v2_dup ( b.b2 );
			const auto	a1 = simd::v2_dup ( b.a1 );
			const auto	a2 = simd::v2_dup ( b.a2 );

			for ( auto s = 0; s < n; ++s )
			{
				const auto	x = simd::v2_load ( buf + 2 * s );
				const auto	y = simd::v2_fma ( z1, b0, x );
				z1 = simd::v2_fms ( simd::v2_fma ( z2, b1, x ), a1, y );
				z2 = simd::v2_fms ( simd::v2_mul ( b2, x ), a2, y );
				simd::v2_store ( buf + 2 * s, y );
			}
		}

		simd::v2_store ( b.z1, z1 );
		simd::v2_store ( b.z2, z2 );
	}

	struct Coeffs { double b0, b1, b2, a1, a2; };

	void updateCoeffs ( int band )
	{
		auto&	b = bands[ band ];

		Coeffs c;

		computeCoeffs ( b.type, b.freq, b.currentDb, b.Q, c );

		b.b0 = c.b0;
		b.b1 = c.b1;
		b.b2 = c.b2;

		b.a1 = c.a1;
		b.a2 = c.a2;
	}

	void computeCoeffs ( type type, double freq, double gainDb, double Q, Coeffs& b ) const
	{
		const auto	A = std::pow ( 10.0, gainDb / 40.0 );
		const auto	w0 = 2.0 * std::numbers::pi * freq / fs;
		const auto	cosW = std::cos ( w0 );
		const auto	sinW = std::sin ( w0 );
		const auto	alpha = sinW / ( 2.0 * Q );

		auto	a0 = 0.0;
		if ( type == peak )
		{
			b.b0 = 1.0 + alpha * A;
			b.b1 = -2.0 * cosW;
			b.b2 = 1.0 - alpha * A;
			a0 = 1.0 + alpha / A;
			b.a1 = -2.0 * cosW;
			b.a2 = 1.0 - alpha / A;
		}
		else if ( type == lowPass || type == highPass )
		{
			// gainDb acts as output gain (A^2 = 10^(gainDb/20)), so the
			// band's gain smoothing still applies; sign selects low/high
			const auto	g = A * A;
			const auto	sign = ( type == lowPass ) ? 1.0 : -1.0;
			const auto	h = g * ( 1.0 - sign * cosW ) * 0.5;

			b.b0 = h;
			b.b1 = sign * 2.0 * h;
			b.b2 = h;
			a0 = 1.0 + alpha;
			b.a1 = -2.0 * cosW;
			b.a2 = 1.0 - alpha;
		}
		else // shelves, sign selects low/high
		{
			const auto	sqA = std::sqrt ( A );
			const auto	twoSA = 2.0 * sqA * alpha;
			const auto	sign = ( type == lowShelf ) ? 1.0 : -1.0;

			b.b0 = A * ( ( A + 1 ) - sign * ( A - 1 ) * cosW + twoSA );
			b.b1 = sign * 2 * A * ( ( A - 1 ) - sign * ( A + 1 ) * cosW );
			b.b2 = A * ( ( A + 1 ) - sign * ( A - 1 ) * cosW - twoSA );
			a0 = ( A + 1 ) + sign * ( A - 1 ) * cosW + twoSA;
			b.a1 = sign * -2 * ( ( A - 1 ) + sign * ( A + 1 ) * cosW );
			b.a2 = ( A + 1 ) + sign * ( A - 1 ) * cosW - twoSA;
		}

		b.b0 /= a0;
		b.b1 /= a0;
		b.b2 /= a0;

		b.a1 /= a0;
		b.a2 /= a0;
	}

	double	fs = 44100.0;
	double	smoothingMs = 30.0;
	double	smoothCoeff = 0.0;

	std::vector<Biquad>	bands;
};
//-----------------------------------------------------------------------------
