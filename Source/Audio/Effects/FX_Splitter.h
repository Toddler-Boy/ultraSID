#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>

#include "FX_Helpers.h"

//
// Splitter, so all the FX only work on the higher frequencies, protecting the bass and leaving it mono
//
// Linkwitz-Riley crossover: an LR filter of order 2N is a squared Butterworth of
// order N, realized as a cascade of N RBJ biquads carrying the squared prototype's
// Q values. Fixed at LR8 (48 dB/oct), four biquads per band. Low and high band
// share their poles, so the recurrence coefficients a1/a2 are identical and only
// the numerators differ. Even order, so no polarity inversion is needed for low +
// high to sum to a flat allpass.
//
// SIMD: both channels travel as one 2-lane double vector (SSE2/NEON), and the
// low/high cascades are two independent dependency chains inside one sample
// loop, so the CPU overlaps them. The stage count is a template parameter,
// giving a fully unrolled, branch-free kernel.
//

class FX_Splitter final
{
public:
	FX_Splitter ()
	{
		updateCoeffs ();
	}
	//-----------------------------------------------------------------------------

	// Splits in place: high band stays in srcDst, low band lands in lowBuffer
	// (both channels, mono input duplicates). numSamples must stay <= maxBlock.
	void splitBands ( float* const* __restrict__ srcDst, const int numSamples, const int numChannels )
	{
		// Coefficients are derived here, never in the setters: they are the one piece of
		// filter state a caller could otherwise rewrite from under an in-flight block
		if ( coeffsDirty.exchange ( false, std::memory_order_acquire ) )
			updateCoeffs ();

		split<numStages> ( srcDst, numSamples, numChannels );
	}
	//-----------------------------------------------------------------------------

	void mergeBands ( float* const* __restrict__ srcDst, const int numSamples )
	{
		for ( auto ch = 0; ch < 2; ++ch )
		{
			auto* __restrict__			dst = srcDst[ ch ];
			const auto* __restrict__	low = lowBuffer[ ch ];

			for ( auto i = 0; i < numSamples; ++i )
				dst[ i ] += low[ i ] * lowGain;
		}
	}
	//-----------------------------------------------------------------------------

	// The one explicit band-balance knob: bass level relative to the processed
	// high band, applied at the merge. The send-style FX (widener, delay,
	// reverb) deliberately ADD energy to the high band; this is where that is
	// accounted for, in one place, in dB. 0 = neutral.
	void setLowGain ( const float db )
	{
		lowGain = fx::helpers::dbToLin ( db );
	}
	//-----------------------------------------------------------------------------

	void reset ()
	{
		for ( auto k = 0; k < numStages; ++k )
			for ( auto ch = 0; ch < 2; ++ch )
				lowZ1[ k ][ ch ] = lowZ2[ k ][ ch ] = highZ1[ k ][ ch ] = highZ2[ k ][ ch ] = 0.0;
	}
	//-----------------------------------------------------------------------------

	// The low bound admits parking the crossover below the audible band (the
	// mode transitions sweep it there, phase-coherently bypassing the split)
	void setFrequency ( const float frequency )
	{
		const auto	f = std::clamp ( frequency, 5.0f, 20000.0f );
		if ( fast::isEqual ( f, freq ) )
			return;

		freq = f;
		coeffsDirty.store ( true, std::memory_order_release );
	}
	//-----------------------------------------------------------------------------


private:
	static constexpr auto	sampleRate = 44100.0;
	static constexpr int	numStages = 4;
	static constexpr int	maxBlock = 1024;

	using V2 = simd::V2;	// shared 2-lane double shim from FX_Helpers.h

	// noinline keeps the kernel's codegen independent of whatever surrounds the
	// call site: inlined into a larger function it competes for registers and
	// can lose 30-60% to spills, depending on unrelated code in the same TU
	template<int NumStages>
	fxnoinline void split ( float* const* __restrict__ srcDst, const int numSamples, const int numChannels )
	{
		// mono reads channel 0 twice, both lanes then carry identical values
		auto* const	chL = srcDst[ 0 ];
		auto* const	chR = srcDst[ numChannels > 1 ? 1 : 0 ];

		V2	lb0[ NumStages ], lb1[ NumStages ], lb2[ NumStages ];
		V2	hb0[ NumStages ], hb1[ NumStages ], hb2[ NumStages ];
		V2	a1[ NumStages ], a2[ NumStages ];
		V2	lz1[ NumStages ], lz2[ NumStages ], hz1[ NumStages ], hz2[ NumStages ];

		for ( auto k = 0; k < NumStages; ++k )
		{
			const auto&	c = coef[ k ];
			lb0[ k ] = simd::v2_dup ( c.lb0 );	lb1[ k ] = simd::v2_dup ( c.lb1 );	lb2[ k ] = simd::v2_dup ( c.lb2 );
			hb0[ k ] = simd::v2_dup ( c.hb0 );	hb1[ k ] = simd::v2_dup ( c.hb1 );	hb2[ k ] = simd::v2_dup ( c.hb2 );
			a1[ k ] = simd::v2_dup ( c.a1 );	a2[ k ] = simd::v2_dup ( c.a2 );

			lz1[ k ] = simd::v2_load ( lowZ1[ k ] );	lz2[ k ] = simd::v2_load ( lowZ2[ k ] );
			hz1[ k ] = simd::v2_load ( highZ1[ k ] );	hz2[ k ] = simd::v2_load ( highZ2[ k ] );
		}

		for ( auto s = 0; s < numSamples; ++s )
		{
			const auto	x = simd::v2_set ( chL[ s ], chR[ s ] );
			auto	lo = x;
			auto	hi = x;

			for ( auto k = 0; k < NumStages; ++k )	// compile-time trip count, fully unrolled
			{
				// transposed direct form II, one biquad per band and stage
				const auto	ly = simd::v2_fma ( lz1[ k ], lb0[ k ], lo );							// lb0*lo + lz1
				lz1[ k ] = simd::v2_fms ( simd::v2_fma ( lz2[ k ], lb1[ k ], lo ), a1[ k ], ly );	// lb1*lo + lz2 - a1*ly
				lz2[ k ] = simd::v2_fms ( simd::v2_mul ( lb2[ k ], lo ), a2[ k ], ly );				// lb2*lo - a2*ly
				lo = ly;

				const auto	hy = simd::v2_fma ( hz1[ k ], hb0[ k ], hi );
				hz1[ k ] = simd::v2_fms ( simd::v2_fma ( hz2[ k ], hb1[ k ], hi ), a1[ k ], hy );
				hz2[ k ] = simd::v2_fms ( simd::v2_mul ( hb2[ k ], hi ), a2[ k ], hy );
				hi = hy;
			}

			lowBuffer[ 0 ][ s ] = static_cast<float>( simd::v2_lane0 ( lo ) );
			lowBuffer[ 1 ][ s ] = static_cast<float>( simd::v2_lane1 ( lo ) );
			chL[ s ] = static_cast<float>( simd::v2_lane0 ( hi ) );
			chR[ s ] = static_cast<float>( simd::v2_lane1 ( hi ) );
		}

		for ( auto k = 0; k < NumStages; ++k )
		{
			simd::v2_store ( lowZ1[ k ], lz1[ k ] );	simd::v2_store ( lowZ2[ k ], lz2[ k ] );
			simd::v2_store ( highZ1[ k ], hz1[ k ] );	simd::v2_store ( highZ2[ k ], hz2[ k ] );
		}
	}
	//-----------------------------------------------------------------------------

	void updateCoeffs ()
	{
		// LR8's Q per stage: the Butterworth prototype's values, each pole pair
		// doubled by the squaring
		static constexpr double	qTable[ numStages ] =
		{
			0.54119610014619699, 0.54119610014619699,
			1.30656296487637653, 1.30656296487637653,
		};

		const auto	w0 = 2.0 * std::numbers::pi * freq / sampleRate;
		const auto	cosW = std::cos ( w0 );
		const auto	sinW = std::sin ( w0 );

		for ( auto k = 0; k < numStages; ++k )
		{
			const auto	alpha = sinW / ( 2.0 * qTable[ k ] );
			const auto	norm = 1.0 / ( 1.0 + alpha );
			const auto	lp = ( 1.0 - cosW ) * 0.5 * norm;
			const auto	hp = ( 1.0 + cosW ) * 0.5 * norm;

			auto&	c = coef[ k ];
			c.lb0 = lp;		c.lb1 = 2.0 * lp;	c.lb2 = lp;
			c.hb0 = hp;		c.hb1 = -2.0 * hp;	c.hb2 = hp;
			c.a1 = -2.0 * cosW * norm;
			c.a2 = ( 1.0 - alpha ) * norm;
		}
	}
	//-----------------------------------------------------------------------------

	struct Stage
	{
		double	lb0, lb1, lb2;		// lowpass numerator
		double	hb0, hb1, hb2;		// highpass numerator
		double	a1, a2;				// shared recurrence
	};

	Stage	coef[ numStages ];

	alignas( 16 ) double	lowZ1[ numStages ][ 2 ] = {};	// [stage][L,R]
	alignas( 16 ) double	lowZ2[ numStages ][ 2 ] = {};
	alignas( 16 ) double	highZ1[ numStages ][ 2 ] = {};
	alignas( 16 ) double	highZ2[ numStages ][ 2 ] = {};

	float	lowBuffer[ 2 ][ maxBlock ] = {};
	float	lowGain = 1.0f;
	float	freq = 200.0f;		// the app syncs the preset value every block anyway

	std::atomic<bool>	coeffsDirty { false };
};
//-----------------------------------------------------------------------------
