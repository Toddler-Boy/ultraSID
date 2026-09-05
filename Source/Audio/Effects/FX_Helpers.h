#pragma once

#include <cassert>
#include <cmath>

#include "libSidplayEZ/src/EZ/config.h"

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__INTEL_COMPILER)
	#define fxinline __forceinline
	#define fxnoinline __declspec(noinline)
#else
	#define fxinline inline __attribute__((always_inline))
	#define fxnoinline __attribute__((noinline))
#endif

#if defined(__aarch64__) && defined(__ARM_NEON)
	#include <arm_neon.h>
	#define FX_SIMD_NEON 1
#elif (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
	#include <emmintrin.h>
	#define FX_SIMD_SSE2 1
#endif

// 2-lane double vector shim (SSE2/NEON), enough for stereo DSP that keeps its
// state in [L,R] pairs. Loads and stores are unaligned-safe, so state may live
// in plain double[2] members. Used by MultiBandEQ and FX_Splitter.
namespace simd
{
#if FX_SIMD_NEON
	using V2 = float64x2_t;
	[[ nodiscard ]] fxinline V2 v2_load ( const double* p )			{	return vld1q_f64 ( p );		}
	fxinline void v2_store ( double* p, V2 v )						{	vst1q_f64 ( p, v );			}
	[[ nodiscard ]] fxinline V2 v2_dup ( double x )					{	return vdupq_n_f64 ( x );	}
	[[ nodiscard ]] fxinline V2 v2_set ( double l, double r )		{	return vcombine_f64 ( vdup_n_f64 ( l ), vdup_n_f64 ( r ) );	}
	[[ nodiscard ]] fxinline double v2_lane0 ( V2 v )				{	return vgetq_lane_f64 ( v, 0 );	}
	[[ nodiscard ]] fxinline double v2_lane1 ( V2 v )				{	return vgetq_lane_f64 ( v, 1 );	}
	[[ nodiscard ]] fxinline V2 v2_mul ( V2 a, V2 b )				{	return vmulq_f64 ( a, b );	}
	[[ nodiscard ]] fxinline V2 v2_fma ( V2 acc, V2 a, V2 b )		{	return vfmaq_f64 ( acc, a, b );	}	// acc + a*b
	[[ nodiscard ]] fxinline V2 v2_fms ( V2 acc, V2 a, V2 b )		{	return vfmsq_f64 ( acc, a, b );	}	// acc - a*b
#elif FX_SIMD_SSE2
	using V2 = __m128d;
	[[ nodiscard ]] fxinline V2 v2_load ( const double* p )			{	return _mm_loadu_pd ( p );	}
	fxinline void v2_store ( double* p, V2 v )						{	_mm_storeu_pd ( p, v );		}
	[[ nodiscard ]] fxinline V2 v2_dup ( double x )					{	return _mm_set1_pd ( x );	}
	[[ nodiscard ]] fxinline V2 v2_set ( double l, double r )		{	return _mm_set_pd ( r, l );	}
	[[ nodiscard ]] fxinline double v2_lane0 ( V2 v )				{	return _mm_cvtsd_f64 ( v );	}
	[[ nodiscard ]] fxinline double v2_lane1 ( V2 v )				{	return _mm_cvtsd_f64 ( _mm_unpackhi_pd ( v, v ) );	}
	[[ nodiscard ]] fxinline V2 v2_mul ( V2 a, V2 b )				{	return _mm_mul_pd ( a, b );	}
	[[ nodiscard ]] fxinline V2 v2_fma ( V2 acc, V2 a, V2 b )		{	return _mm_add_pd ( acc, _mm_mul_pd ( a, b ) );	}	// acc + a*b
	[[ nodiscard ]] fxinline V2 v2_fms ( V2 acc, V2 a, V2 b )		{	return _mm_sub_pd ( acc, _mm_mul_pd ( a, b ) );	}	// acc - a*b
#else
	#error "No SIMD support"
#endif
}
//-----------------------------------------------------------------------------

namespace fast
{
	[[ nodiscard ]] fxinline float lerp ( const float a, const float b, const float t )	{	return a + t * ( b - a );	}
	[[ nodiscard ]] fxinline float pow2 ( const float a ) { return a * a;  }
	[[ nodiscard ]] fxinline float tanh ( const float x )
	{
		const auto	ax = std::fabs ( x );
		const auto	x2 = x * x;
		const auto	z = x * ( 0.773062670268356f + ax + ( 0.757118539838817f + 0.0139332362248817f * x2 * x2 ) * x2 * ax );

		return z / ( 0.795956503022967f + std::fabs ( z ) );
	}

	[[ nodiscard ]] fxinline float sin ( const float x )
	{
		return -0.000182690409228785f * x * x * x * x * x * x * x
				+ 0.00830460224186793f * x * x * x * x * x
				- 0.166651012143690f * x * x * x
				+ x;
	}

	[[ nodiscard ]] fxinline bool isEqual ( const float a, const float b )	{	return std::abs ( a - b ) < 1e-6f;	}
}
//-----------------------------------------------------------------------------

namespace fx::helpers
{
	constexpr auto dbToLin = [] ( float db ) -> float { return std::pow ( 10.0f, db * 0.05f ); };

	// CPU SIMD-level detection lives in libSidplayEZ (EZ/config.h), aliased here
	// so FX code keeps its fx::helpers:: spelling without a second copy to maintain
	using libsidplayEZ::SimdLevel;
	using libsidplayEZ::SIMD_SSE42;
	using libsidplayEZ::SIMD_AVX;
	using libsidplayEZ::SIMD_AVX2;
	using libsidplayEZ::SIMD_NEON;
	using libsidplayEZ::simdLevel;
}
//-----------------------------------------------------------------------------

class SmoothedValue
{
public:
	SmoothedValue ( const float initVal = 0.0f ) noexcept { setAndSnap ( initVal ); }

	void set ( const float val ) noexcept { dstValue = val; }
	[[ nodiscard ]] float get () const noexcept { return curValue; }
	void snap () noexcept	{ curValue = dstValue; }

	void step () noexcept { curValue = fast::lerp ( curValue, dstValue, fast ); }
	void stepSlow () noexcept { curValue = fast::lerp ( curValue, dstValue, slow ); }

	void setAndSnap ( const float val ) {
		set ( val );
		snap ();
	}

	[[ nodiscard ]] float getAndStep () noexcept {
		const auto	ret = get ();
		step ();
		return ret;
	}

	[[ nodiscard ]] float getAndStepSlow () noexcept {
		const auto	ret = get ();
		stepSlow ();
		return ret;
	}

	static constexpr auto	minVolume = 1.0f / 65535.0f;
	static constexpr auto	normVolume = 1.0f - minVolume;

	// restingAtZero/One assume targets in [0, 1] (fades, volumes); unbounded
	// values (gains above 1.0) must use restingAtTarget
	[[ nodiscard ]] bool restingAtZero () const noexcept { assert ( dstValue >= 0.0f && dstValue <= 1.0f ); return dstValue < minVolume && curValue < minVolume; }
	[[ nodiscard ]] bool restingAtOne () const noexcept { assert ( dstValue >= 0.0f && dstValue <= 1.0f ); return dstValue >= normVolume && curValue >= normVolume; }
	[[ nodiscard ]] bool restingAtTarget () const noexcept { return std::abs ( dstValue - curValue ) < minVolume; }

private:
	static constexpr auto	fast = ( 1.0f / ( 44.1f * 3.0234f ) );

	// Deliberately ~1 s: replay-gain corrections during listening must stay
	// gentle, never abrupt
	static constexpr auto	slow = ( 1.0f / ( 44.1f * 1000.0f ) );

	float	curValue = 0.0f;
	float	dstValue = 0.0f;
};
//-----------------------------------------------------------------------------
