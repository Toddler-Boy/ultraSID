#pragma once

#include "FX_Helpers.h"
#include "MultiBandEQ.h"

#if defined(__aarch64__) && defined(__ARM_NEON)
	#include <arm_neon.h>
	#define CTVS_NEON 1
#elif (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
	#include <immintrin.h>
	#define CTVS_SSE2 1
	// the TU is compiled for the SSE4.2 baseline, so the AVX2 kernel needs its own target
	#if defined(__clang__) || defined(__GNUC__)
		#define CTVS_TARGET_AVX2 __attribute__(( target( "avx2,fma" ) ))
	#else
		#define CTVS_TARGET_AVX2
	#endif
#endif

//-----------------------------------------------------------------------------

// Simulates a low-quality 2-inch TV speaker: a terrible EQ curve followed by
// tanh distortion, so the resonance peaks overdrive first when played loud.
class FX_CheapTVSpeaker final
{
public:
	FX_CheapTVSpeaker ()
	{
		eq.setNumBands ( 10 );

		eq.setBand ( 0, MultiBandEQ::type::highPass,	 180.0,    0.0, 1.5 );
		eq.setBand ( 1, MultiBandEQ::type::highPass,	 180.0,    0.0, 1.5 );
		eq.setBand ( 2, MultiBandEQ::type::peak,		 125.0,   -7.0, 1.41 );
		eq.setBand ( 3, MultiBandEQ::type::peak,		 250.0,    5.0, 1.41 );
		eq.setBand ( 4, MultiBandEQ::type::peak,		 500.0,   -7.0, 1.41 );
		eq.setBand ( 5, MultiBandEQ::type::peak,		1000.0,   10.0, 1.41 );
		eq.setBand ( 6, MultiBandEQ::type::peak,		2000.0,   -5.0, 1.41 );
		eq.setBand ( 7, MultiBandEQ::type::peak,		4000.0,   14.0, 1.41 );
		eq.setBand ( 8, MultiBandEQ::type::peak,		8000.0,   -7.0, 1.41 );
		eq.setBand ( 9, MultiBandEQ::type::lowPass,  16000.0,    0.0, 1.0 );
	}

	void process ( float* const* __restrict srcDst, const int numSamples )
	{
		eq.process ( srcDst[ 0 ], srcDst[ 1 ], numSamples );

		const auto	distGainInv = 1.0f / distGain;

		for ( auto ch = 0; ch < 2; ++ch )
		{
			auto* __restrict	buf = srcDst[ ch ];

			auto	i = 0;
		#if CTVS_SSE2
			if ( useAVX2 )
				i = tanh8Bulk ( buf, numSamples, distGain, distGainInv );
		#endif
			for ( ; i + 4 <= numSamples; i += 4 )
				tanh4 ( buf + i, distGain, distGainInv );

			// at most 3 samples left, stop clang from emitting a dead vectorized copy
			#if defined(__clang__)
				#pragma clang loop vectorize(disable) unroll(disable)
			#endif
			for ( ; i < numSamples; ++i )
				buf[ i ] = fast::tanh ( buf[ i ] * distGain ) * distGainInv;
		}
	}

	void setDistortion ( const float gain )
	{
		distGain = gain;
	}

	void clearBuffers ()
	{
		eq.reset ();
	}

private:
	// 4-lane fast::tanh with the division replaced by a reciprocal estimate
	// plus Newton-Raphson refinement (to ~full float precision). The scalar
	// tail above still divides; the ~1 ulp lane/tail mismatch is inaudible.
#if CTVS_NEON
	static void tanh4 ( float* __restrict p, const float gain, const float gainInv )
	{
		const auto	x = vmulq_n_f32 ( vld1q_f32 ( p ), gain );
		const auto	ax = vabsq_f32 ( x );
		const auto	x2 = vmulq_f32 ( x, x );
		const auto	x4 = vmulq_f32 ( x2, x2 );

		auto	t = vfmaq_f32 ( vdupq_n_f32 ( 0.757118539838817f ), vdupq_n_f32 ( 0.0139332362248817f ), x4 );
		t = vmulq_f32 ( vmulq_f32 ( t, x2 ), ax );
		t = vaddq_f32 ( vaddq_f32 ( vdupq_n_f32 ( 0.773062670268356f ), ax ), t );

		const auto	z = vmulq_f32 ( x, t );
		const auto	den = vaddq_f32 ( vdupq_n_f32 ( 0.795956503022967f ), vabsq_f32 ( z ) );

		auto	r = vrecpeq_f32 ( den );                 // ~8 bit estimate
		r = vmulq_f32 ( vrecpsq_f32 ( den, r ), r );    // -> ~16 bit
		r = vmulq_f32 ( vrecpsq_f32 ( den, r ), r );    // -> ~full precision

		vst1q_f32 ( p, vmulq_n_f32 ( vmulq_f32 ( z, r ), gainInv ) );
	}
#elif CTVS_SSE2
	static void tanh4 ( float* __restrict p, const float gain, const float gainInv )
	{
		const auto	absMask = _mm_castsi128_ps ( _mm_set1_epi32 ( 0x7FFFFFFF ) );

		const auto	x = _mm_mul_ps ( _mm_loadu_ps ( p ), _mm_set1_ps ( gain ) );
		const auto	ax = _mm_and_ps ( x, absMask );
		const auto	x2 = _mm_mul_ps ( x, x );
		const auto	x4 = _mm_mul_ps ( x2, x2 );

		auto	t = _mm_add_ps ( _mm_set1_ps ( 0.757118539838817f ), _mm_mul_ps ( _mm_set1_ps ( 0.0139332362248817f ), x4 ) );
		t = _mm_mul_ps ( _mm_mul_ps ( t, x2 ), ax );
		t = _mm_add_ps ( _mm_add_ps ( _mm_set1_ps ( 0.773062670268356f ), ax ), t );

		const auto	z = _mm_mul_ps ( x, t );
		const auto	den = _mm_add_ps ( _mm_set1_ps ( 0.795956503022967f ), _mm_and_ps ( z, absMask ) );

		auto	r = _mm_rcp_ps ( den );                                                     // ~12 bit estimate
		r = _mm_mul_ps ( r, _mm_sub_ps ( _mm_set1_ps ( 2.0f ), _mm_mul_ps ( den, r ) ) );   // -> ~full precision

		_mm_storeu_ps ( p, _mm_mul_ps ( _mm_mul_ps ( z, r ), _mm_set1_ps ( gainInv ) ) );
	}

	// 8-lane AVX2+FMA variant of the same math. Runs the whole bulk in one
	// call so the compiler emits a single vzeroupper per block instead of
	// paying the AVX/SSE transition per 8 samples. Returns samples consumed.
	[[ nodiscard ]] CTVS_TARGET_AVX2 static int tanh8Bulk ( float* __restrict p, const int numSamples, const float gain, const float gainInv )
	{
		const auto	absMask = _mm256_castsi256_ps ( _mm256_set1_epi32 ( 0x7FFFFFFF ) );
		const auto	vGain = _mm256_set1_ps ( gain );
		const auto	vGainInv = _mm256_set1_ps ( gainInv );
		const auto	c0 = _mm256_set1_ps ( 0.773062670268356f );
		const auto	c1 = _mm256_set1_ps ( 0.757118539838817f );
		const auto	c2 = _mm256_set1_ps ( 0.0139332362248817f );
		const auto	c3 = _mm256_set1_ps ( 0.795956503022967f );
		const auto	two = _mm256_set1_ps ( 2.0f );

		auto	i = 0;
		for ( ; i + 8 <= numSamples; i += 8 )
		{
			const auto	x = _mm256_mul_ps ( _mm256_loadu_ps ( p + i ), vGain );
			const auto	ax = _mm256_and_ps ( x, absMask );
			const auto	x2 = _mm256_mul_ps ( x, x );
			const auto	x4 = _mm256_mul_ps ( x2, x2 );

			auto	t = _mm256_fmadd_ps ( c2, x4, c1 );
			t = _mm256_mul_ps ( _mm256_mul_ps ( t, x2 ), ax );
			t = _mm256_add_ps ( _mm256_add_ps ( c0, ax ), t );

			const auto	z = _mm256_mul_ps ( x, t );
			const auto	den = _mm256_add_ps ( c3, _mm256_and_ps ( z, absMask ) );

			auto	r = _mm256_rcp_ps ( den );                          // ~12 bit estimate
			r = _mm256_mul_ps ( r, _mm256_fnmadd_ps ( den, r, two ) ); // -> ~full precision

			_mm256_storeu_ps ( p + i, _mm256_mul_ps ( _mm256_mul_ps ( z, r ), vGainInv ) );
		}
		return i;
	}
#else
	#error "No SIMD support"
#endif

	MultiBandEQ	eq;
	float		distGain = 1.75f;

#if CTVS_SSE2
	const bool	useAVX2 = ( fx::helpers::simdLevel () >= fx::helpers::SIMD_AVX2 );
#endif
};
//-----------------------------------------------------------------------------
