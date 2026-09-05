//	Copyright (c) 2010 Martin Eastwood
//  This code is distributed under the terms of the GNU General Public License

//  MVerb is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  at your option) any later version.
//
//  MVerb is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this MVerb.  If not, see <http://www.gnu.org/licenses/>.

//  2026, modified for ultraSID (same license): ~4.5x faster than the original
//  per-sample code (~1300x realtime, 44.1 kHz stereo, one Zen 4 core, AVX2).
//   - block-based processing with SIMD kernels (SSE baseline, runtime-dispatched
//     AVX2+FMA via built-in CPU detection, NEON on aarch64); the header is
//     self-contained, no dependencies beyond the standard library
//   - the OverSampleCount-times-iterated state-variable filter loop is folded
//     into a single precomputed affine step (StateVariable::Tick), and filter
//     coefficients update at chunk rate with an unchanged-value early-out
//   - the original per-sample loop is kept as a scalar fallback (processScalar);
//     equivalent to well below audibility (float-ulp once parameters settle)

#ifndef EMVERB_H
#define EMVERB_H

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

#if defined(__aarch64__) && defined(__ARM_NEON)
	#include <arm_neon.h>
	#define MVERB_NEON 1
#elif (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
	#include <immintrin.h>
	#if defined(_MSC_VER)
		#include <intrin.h>		// __cpuid / __cpuidex / _xgetbv (MSVC and clang-cl)
	#else
		#include <cpuid.h>
	#endif
	#define MVERB_SSE 1
	// the TU is compiled for the SSE4.2 baseline, so the AVX2 kernels need their own target
	#if defined(__clang__) || defined(__GNUC__)
		#define MVERB_TARGET_AVX2 __attribute__(( target( "avx2,fma" ) ))
	#else
		#define MVERB_TARGET_AVX2
	#endif
#endif

//-----------------------------------------------------------------------------
// Small float-array kernels for the block-based MVerb process path. n is tiny
// (<= 32) and the buffers are stack scratch or L1-resident delay-line memory,
// so these are throughput-, not bandwidth-bound. SSE baseline with an AVX2+FMA
// variant selected at runtime, NEON on aarch64, plain scalar otherwise.
namespace mverb_kernels
{
#if MVERB_SSE
	// Runtime AVX2+FMA detection (CPU support AND OS state saving on context
	// switch), built in so the header has no external dependencies.
	inline bool detectAVX2 ()
	{
		int	r[ 4 ];

	#if defined(_MSC_VER)
		__cpuid ( r, 0 );
		const auto	maxLeaf = r[ 0 ];
		__cpuid ( r, 1 );
	#else
		__get_cpuid ( 0, (unsigned*)&r[ 0 ], (unsigned*)&r[ 1 ], (unsigned*)&r[ 2 ], (unsigned*)&r[ 3 ] );
		const auto	maxLeaf = r[ 0 ];
		__get_cpuid ( 1, (unsigned*)&r[ 0 ], (unsigned*)&r[ 1 ], (unsigned*)&r[ 2 ], (unsigned*)&r[ 3 ] );
	#endif

		const auto	ecx1 = r[ 2 ];
		const auto	fma = ( ecx1 & ( 1 << 12 ) ) != 0;
		const auto	avx = ( ecx1 & ( 1 << 28 ) ) != 0;
		const auto	osxsave = ( ecx1 & ( 1 << 27 ) ) != 0;
		if ( !fma || !avx || !osxsave || maxLeaf < 7 )
			return false;

		unsigned long long	xcr0 = 0;
	#if defined(_MSC_VER)
		xcr0 = _xgetbv ( 0 );
	#else
		// the builtin needs -mxsave at compile time, so use the raw instruction
		unsigned	lo, hi;
		__asm__ __volatile__ ( "xgetbv" : "=a"( lo ), "=d"( hi ) : "c"( 0 ) );
		xcr0 = ( (unsigned long long)hi << 32 ) | lo;
	#endif
		if ( ( xcr0 & 0x06 ) != 0x06 )		// OS saves XMM + YMM state
			return false;

	#if defined(_MSC_VER)
		__cpuidex ( r, 7, 0 );
	#else
		__get_cpuid_count ( 7, 0, (unsigned*)&r[ 0 ], (unsigned*)&r[ 1 ], (unsigned*)&r[ 2 ], (unsigned*)&r[ 3 ] );
	#endif
		return ( r[ 1 ] & ( 1 << 5 ) ) != 0;	// AVX2
	}

	// cached once, cheap to call from every constructor
	inline bool hasAVX2 ()
	{
		static const bool	avx2 = detectAVX2 ();
		return avx2;
	}
#endif // MVERB_SSE

	// out[ 0..n ) = k * src[ 0..n )
	inline void firSetScalar ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		for ( auto i = 0; i < n; ++i )
			out[ i ] = k * src[ i ];
	}

	// out[ 0..n ) += k * src[ 0..n )
	inline void firAccScalar ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		for ( auto i = 0; i < n; ++i )
			out[ i ] += k * src[ i ];
	}

	// out[ 0..n ) = a[ 0..n ) + b[ 0..n )
	inline void addVScalar ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		for ( auto i = 0; i < n; ++i )
			out[ i ] = a[ i ] + b[ i ];
	}

	// out[ 0..n ) = a[ 0..n ) * b[ 0..n )
	inline void mulVScalar ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		for ( auto i = 0; i < n; ++i )
			out[ i ] = a[ i ] * b[ i ];
	}

	// in-place allpass pass over one contiguous run (same math as the per-sample
	// classes): bo = buf[ i ]; o = bo - g * x[ i ]; buf[ i ] = x[ i ] + g * o; x[ i ] = o
	inline void allpassRunScalar ( float* __restrict buf, float* __restrict x, const float g, const int n )
	{
		for ( auto i = 0; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g * x[ i ];
			buf[ i ] = x[ i ] + g * o;
			x[ i ] = o;
		}
	}

	// same, with a per-sample feedback array
	inline void allpassRunVScalar ( float* __restrict buf, float* __restrict x, const float* __restrict g, const int n )
	{
		for ( auto i = 0; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g[ i ] * x[ i ];
			buf[ i ] = x[ i ] + g[ i ] * o;
			x[ i ] = o;
		}
	}

	// out[ i ] = ( in[ i ] + mix_i * ( ( earlyMix * acc[ i ] + lateMix * er[ i ] ) - in[ i ] ) ) * gain
	// with mix_i = mixBase + ( i + 1 ) * mixDelta, the dry/wet crossfade
	// (no __restrict on out/in: the reverb runs in place, so they may be the same buffer)
	inline void mixOutScalar ( float* out, const float* in, const float* __restrict acc, const float* __restrict er,
							   const float earlyMix, const float lateMix, const float mixBase, const float mixDelta,
							   const float gain, const int n )
	{
		for ( auto i = 0; i < n; ++i )
		{
			const auto	mix = mixBase + static_cast<float>( i + 1 ) * mixDelta;
			const auto	acu = acc[ i ] * earlyMix + lateMix * er[ i ];
			out[ i ] = ( in[ i ] + mix * ( acu - in[ i ] ) ) * gain;
		}
	}

#if MVERB_SSE
	inline void firSetSSE ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		const auto	vk = _mm_set1_ps ( k );
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			_mm_storeu_ps ( out + i, _mm_mul_ps ( vk, _mm_loadu_ps ( src + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = k * src[ i ];
	}

	inline void firAccSSE ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		const auto	vk = _mm_set1_ps ( k );
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			_mm_storeu_ps ( out + i, _mm_add_ps ( _mm_loadu_ps ( out + i ), _mm_mul_ps ( vk, _mm_loadu_ps ( src + i ) ) ) );
		for ( ; i < n; ++i )
			out[ i ] += k * src[ i ];
	}

	inline void addVSSE ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			_mm_storeu_ps ( out + i, _mm_add_ps ( _mm_loadu_ps ( a + i ), _mm_loadu_ps ( b + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = a[ i ] + b[ i ];
	}

	inline void mulVSSE ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			_mm_storeu_ps ( out + i, _mm_mul_ps ( _mm_loadu_ps ( a + i ), _mm_loadu_ps ( b + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = a[ i ] * b[ i ];
	}

	inline void allpassRunSSE ( float* __restrict buf, float* __restrict x, const float g, const int n )
	{
		const auto	vg = _mm_set1_ps ( g );
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
		{
			const auto	bo = _mm_loadu_ps ( buf + i );
			const auto	xv = _mm_loadu_ps ( x + i );
			const auto	o = _mm_sub_ps ( bo, _mm_mul_ps ( vg, xv ) );
			_mm_storeu_ps ( buf + i, _mm_add_ps ( xv, _mm_mul_ps ( vg, o ) ) );
			_mm_storeu_ps ( x + i, o );
		}
		for ( ; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g * x[ i ];
			buf[ i ] = x[ i ] + g * o;
			x[ i ] = o;
		}
	}

	inline void allpassRunVSSE ( float* __restrict buf, float* __restrict x, const float* __restrict g, const int n )
	{
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
		{
			const auto	vg = _mm_loadu_ps ( g + i );
			const auto	bo = _mm_loadu_ps ( buf + i );
			const auto	xv = _mm_loadu_ps ( x + i );
			const auto	o = _mm_sub_ps ( bo, _mm_mul_ps ( vg, xv ) );
			_mm_storeu_ps ( buf + i, _mm_add_ps ( xv, _mm_mul_ps ( vg, o ) ) );
			_mm_storeu_ps ( x + i, o );
		}
		for ( ; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g[ i ] * x[ i ];
			buf[ i ] = x[ i ] + g[ i ] * o;
			x[ i ] = o;
		}
	}

	inline void mixOutSSE ( float* out, const float* in, const float* __restrict acc, const float* __restrict er,
							const float earlyMix, const float lateMix, const float mixBase, const float mixDelta,
							const float gain, const int n )
	{
		const auto	vem = _mm_set1_ps ( earlyMix );
		const auto	vlm = _mm_set1_ps ( lateMix );
		const auto	vg = _mm_set1_ps ( gain );
		const auto	vb = _mm_set1_ps ( mixBase );
		const auto	vd = _mm_set1_ps ( mixDelta );
		const auto	ramp = _mm_setr_ps ( 1.0f, 2.0f, 3.0f, 4.0f );

		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
		{
			const auto	idx = _mm_add_ps ( ramp, _mm_set1_ps ( static_cast<float>( i ) ) );
			const auto	mix = _mm_add_ps ( vb, _mm_mul_ps ( idx, vd ) );
			const auto	acu = _mm_add_ps ( _mm_mul_ps ( _mm_loadu_ps ( acc + i ), vem ),
										   _mm_mul_ps ( vlm, _mm_loadu_ps ( er + i ) ) );
			const auto	dry = _mm_loadu_ps ( in + i );
			_mm_storeu_ps ( out + i, _mm_mul_ps ( _mm_add_ps ( dry, _mm_mul_ps ( mix, _mm_sub_ps ( acu, dry ) ) ), vg ) );
		}
		for ( ; i < n; ++i )
		{
			const auto	mix = mixBase + static_cast<float>( i + 1 ) * mixDelta;
			const auto	acu = acc[ i ] * earlyMix + lateMix * er[ i ];
			out[ i ] = ( in[ i ] + mix * ( acu - in[ i ] ) ) * gain;
		}
	}

	MVERB_TARGET_AVX2 inline void firSetAVX2 ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		const auto	vk = _mm256_set1_ps ( k );
		auto	i = 0;
		for ( ; i + 8 <= n; i += 8 )
			_mm256_storeu_ps ( out + i, _mm256_mul_ps ( vk, _mm256_loadu_ps ( src + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = k * src[ i ];
	}

	MVERB_TARGET_AVX2 inline void firAccAVX2 ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		const auto	vk = _mm256_set1_ps ( k );
		auto	i = 0;
		for ( ; i + 8 <= n; i += 8 )
			_mm256_storeu_ps ( out + i, _mm256_fmadd_ps ( vk, _mm256_loadu_ps ( src + i ), _mm256_loadu_ps ( out + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] += k * src[ i ];
	}

	MVERB_TARGET_AVX2 inline void addVAVX2 ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		auto	i = 0;
		for ( ; i + 8 <= n; i += 8 )
			_mm256_storeu_ps ( out + i, _mm256_add_ps ( _mm256_loadu_ps ( a + i ), _mm256_loadu_ps ( b + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = a[ i ] + b[ i ];
	}

	MVERB_TARGET_AVX2 inline void mulVAVX2 ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		auto	i = 0;
		for ( ; i + 8 <= n; i += 8 )
			_mm256_storeu_ps ( out + i, _mm256_mul_ps ( _mm256_loadu_ps ( a + i ), _mm256_loadu_ps ( b + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = a[ i ] * b[ i ];
	}

	MVERB_TARGET_AVX2 inline void allpassRunAVX2 ( float* __restrict buf, float* __restrict x, const float g, const int n )
	{
		const auto	vg = _mm256_set1_ps ( g );
		auto	i = 0;
		for ( ; i + 8 <= n; i += 8 )
		{
			const auto	bo = _mm256_loadu_ps ( buf + i );
			const auto	xv = _mm256_loadu_ps ( x + i );
			const auto	o = _mm256_fnmadd_ps ( vg, xv, bo );			// bo - g*x
			_mm256_storeu_ps ( buf + i, _mm256_fmadd_ps ( vg, o, xv ) );	// x + g*o
			_mm256_storeu_ps ( x + i, o );
		}
		for ( ; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g * x[ i ];
			buf[ i ] = x[ i ] + g * o;
			x[ i ] = o;
		}
	}

	MVERB_TARGET_AVX2 inline void allpassRunVAVX2 ( float* __restrict buf, float* __restrict x, const float* __restrict g, const int n )
	{
		auto	i = 0;
		for ( ; i + 8 <= n; i += 8 )
		{
			const auto	vg = _mm256_loadu_ps ( g + i );
			const auto	bo = _mm256_loadu_ps ( buf + i );
			const auto	xv = _mm256_loadu_ps ( x + i );
			const auto	o = _mm256_fnmadd_ps ( vg, xv, bo );
			_mm256_storeu_ps ( buf + i, _mm256_fmadd_ps ( vg, o, xv ) );
			_mm256_storeu_ps ( x + i, o );
		}
		for ( ; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g[ i ] * x[ i ];
			buf[ i ] = x[ i ] + g[ i ] * o;
			x[ i ] = o;
		}
	}

	MVERB_TARGET_AVX2 inline void mixOutAVX2 ( float* out, const float* in, const float* __restrict acc, const float* __restrict er,
											   const float earlyMix, const float lateMix, const float mixBase, const float mixDelta,
											   const float gain, const int n )
	{
		const auto	vem = _mm256_set1_ps ( earlyMix );
		const auto	vlm = _mm256_set1_ps ( lateMix );
		const auto	vg = _mm256_set1_ps ( gain );
		const auto	vb = _mm256_set1_ps ( mixBase );
		const auto	vd = _mm256_set1_ps ( mixDelta );
		const auto	ramp = _mm256_setr_ps ( 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f );

		auto	i = 0;
		for ( ; i + 8 <= n; i += 8 )
		{
			const auto	idx = _mm256_add_ps ( ramp, _mm256_set1_ps ( static_cast<float>( i ) ) );
			const auto	mix = _mm256_fmadd_ps ( idx, vd, vb );
			const auto	acu = _mm256_fmadd_ps ( _mm256_loadu_ps ( acc + i ), vem,
												_mm256_mul_ps ( vlm, _mm256_loadu_ps ( er + i ) ) );
			const auto	dry = _mm256_loadu_ps ( in + i );
			_mm256_storeu_ps ( out + i, _mm256_mul_ps ( _mm256_fmadd_ps ( mix, _mm256_sub_ps ( acu, dry ), dry ), vg ) );
		}
		for ( ; i < n; ++i )
		{
			const auto	mix = mixBase + static_cast<float>( i + 1 ) * mixDelta;
			const auto	acu = acc[ i ] * earlyMix + lateMix * er[ i ];
			out[ i ] = ( in[ i ] + mix * ( acu - in[ i ] ) ) * gain;
		}
	}
#endif // MVERB_SSE

#if MVERB_NEON
	inline void firSetNEON ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		const auto	vk = vdupq_n_f32 ( k );
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			vst1q_f32 ( out + i, vmulq_f32 ( vk, vld1q_f32 ( src + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = k * src[ i ];
	}

	inline void firAccNEON ( float* __restrict out, const float* __restrict src, const float k, const int n )
	{
		const auto	vk = vdupq_n_f32 ( k );
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			vst1q_f32 ( out + i, vfmaq_f32 ( vld1q_f32 ( out + i ), vk, vld1q_f32 ( src + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] += k * src[ i ];
	}

	inline void addVNEON ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			vst1q_f32 ( out + i, vaddq_f32 ( vld1q_f32 ( a + i ), vld1q_f32 ( b + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = a[ i ] + b[ i ];
	}

	inline void mulVNEON ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n )
	{
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
			vst1q_f32 ( out + i, vmulq_f32 ( vld1q_f32 ( a + i ), vld1q_f32 ( b + i ) ) );
		for ( ; i < n; ++i )
			out[ i ] = a[ i ] * b[ i ];
	}

	inline void allpassRunNEON ( float* __restrict buf, float* __restrict x, const float g, const int n )
	{
		const auto	vg = vdupq_n_f32 ( g );
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
		{
			const auto	bo = vld1q_f32 ( buf + i );
			const auto	xv = vld1q_f32 ( x + i );
			const auto	o = vfmsq_f32 ( bo, vg, xv );			// bo - g*x
			vst1q_f32 ( buf + i, vfmaq_f32 ( xv, vg, o ) );		// x + g*o
			vst1q_f32 ( x + i, o );
		}
		for ( ; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g * x[ i ];
			buf[ i ] = x[ i ] + g * o;
			x[ i ] = o;
		}
	}

	inline void allpassRunVNEON ( float* __restrict buf, float* __restrict x, const float* __restrict g, const int n )
	{
		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
		{
			const auto	vg = vld1q_f32 ( g + i );
			const auto	bo = vld1q_f32 ( buf + i );
			const auto	xv = vld1q_f32 ( x + i );
			const auto	o = vfmsq_f32 ( bo, vg, xv );
			vst1q_f32 ( buf + i, vfmaq_f32 ( xv, vg, o ) );
			vst1q_f32 ( x + i, o );
		}
		for ( ; i < n; ++i )
		{
			const auto	bo = buf[ i ];
			const auto	o = bo - g[ i ] * x[ i ];
			buf[ i ] = x[ i ] + g[ i ] * o;
			x[ i ] = o;
		}
	}

	inline void mixOutNEON ( float* out, const float* in, const float* __restrict acc, const float* __restrict er,
							 const float earlyMix, const float lateMix, const float mixBase, const float mixDelta,
							 const float gain, const int n )
	{
		const auto	vem = vdupq_n_f32 ( earlyMix );
		const auto	vlm = vdupq_n_f32 ( lateMix );
		const auto	vg = vdupq_n_f32 ( gain );
		const auto	vb = vdupq_n_f32 ( mixBase );
		const auto	vd = vdupq_n_f32 ( mixDelta );
		const float32x4_t	ramp = { 1.0f, 2.0f, 3.0f, 4.0f };

		auto	i = 0;
		for ( ; i + 4 <= n; i += 4 )
		{
			const auto	idx = vaddq_f32 ( ramp, vdupq_n_f32 ( static_cast<float>( i ) ) );
			const auto	mix = vfmaq_f32 ( vb, idx, vd );
			const auto	acu = vfmaq_f32 ( vmulq_f32 ( vlm, vld1q_f32 ( er + i ) ), vld1q_f32 ( acc + i ), vem );
			const auto	dry = vld1q_f32 ( in + i );
			vst1q_f32 ( out + i, vmulq_f32 ( vfmaq_f32 ( dry, mix, vsubq_f32 ( acu, dry ) ), vg ) );
		}
		for ( ; i < n; ++i )
		{
			const auto	mix = mixBase + static_cast<float>( i + 1 ) * mixDelta;
			const auto	acu = acc[ i ] * earlyMix + lateMix * er[ i ];
			out[ i ] = ( in[ i ] + mix * ( acu - in[ i ] ) ) * gain;
		}
	}
#endif // MVERB_NEON

	// runtime dispatchers -----------------------------------------------------

	inline void firSet ( float* __restrict out, const float* __restrict src, const float k, const int n, const bool avx2 )
	{
	#if MVERB_SSE
		if ( avx2 )
			firSetAVX2 ( out, src, k, n );
		else
			firSetSSE ( out, src, k, n );
	#elif MVERB_NEON
		(void)avx2;
		firSetNEON ( out, src, k, n );
	#else
		(void)avx2;
		firSetScalar ( out, src, k, n );
	#endif
	}

	inline void firAcc ( float* __restrict out, const float* __restrict src, const float k, const int n, const bool avx2 )
	{
	#if MVERB_SSE
		if ( avx2 )
			firAccAVX2 ( out, src, k, n );
		else
			firAccSSE ( out, src, k, n );
	#elif MVERB_NEON
		(void)avx2;
		firAccNEON ( out, src, k, n );
	#else
		(void)avx2;
		firAccScalar ( out, src, k, n );
	#endif
	}

	inline void mixOut ( float* out, const float* in, const float* __restrict acc, const float* __restrict er,
						 const float earlyMix, const float lateMix, const float mixBase, const float mixDelta,
						 const float gain, const int n, const bool avx2 )
	{
	#if MVERB_SSE
		if ( avx2 )
			mixOutAVX2 ( out, in, acc, er, earlyMix, lateMix, mixBase, mixDelta, gain, n );
		else
			mixOutSSE ( out, in, acc, er, earlyMix, lateMix, mixBase, mixDelta, gain, n );
	#elif MVERB_NEON
		(void)avx2;
		mixOutNEON ( out, in, acc, er, earlyMix, lateMix, mixBase, mixDelta, gain, n );
	#else
		(void)avx2;
		mixOutScalar ( out, in, acc, er, earlyMix, lateMix, mixBase, mixDelta, gain, n );
	#endif
	}

	inline void addV ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n, const bool avx2 )
	{
	#if MVERB_SSE
		if ( avx2 )
			addVAVX2 ( out, a, b, n );
		else
			addVSSE ( out, a, b, n );
	#elif MVERB_NEON
		(void)avx2;
		addVNEON ( out, a, b, n );
	#else
		(void)avx2;
		addVScalar ( out, a, b, n );
	#endif
	}

	inline void mulV ( float* __restrict out, const float* __restrict a, const float* __restrict b, const int n, const bool avx2 )
	{
	#if MVERB_SSE
		if ( avx2 )
			mulVAVX2 ( out, a, b, n );
		else
			mulVSSE ( out, a, b, n );
	#elif MVERB_NEON
		(void)avx2;
		mulVNEON ( out, a, b, n );
	#else
		(void)avx2;
		mulVScalar ( out, a, b, n );
	#endif
	}

	inline void allpassRun ( float* __restrict buf, float* __restrict x, const float g, const int n, const bool avx2 )
	{
	#if MVERB_SSE
		if ( avx2 )
			allpassRunAVX2 ( buf, x, g, n );
		else
			allpassRunSSE ( buf, x, g, n );
	#elif MVERB_NEON
		(void)avx2;
		allpassRunNEON ( buf, x, g, n );
	#else
		(void)avx2;
		allpassRunScalar ( buf, x, g, n );
	#endif
	}

	inline void allpassRunV ( float* __restrict buf, float* __restrict x, const float* __restrict g, const int n, const bool avx2 )
	{
	#if MVERB_SSE
		if ( avx2 )
			allpassRunVAVX2 ( buf, x, g, n );
		else
			allpassRunVSSE ( buf, x, g, n );
	#elif MVERB_NEON
		(void)avx2;
		allpassRunVNEON ( buf, x, g, n );
	#else
		(void)avx2;
		allpassRunVScalar ( buf, x, g, n );
	#endif
	}

	// ring-buffer helpers: (pos + i) wraps at length (length > 0) -------------

	// out[ 0..n ) = k * ring[ ( pos + i ) % length ]
	inline void ringTapSet ( float* __restrict out, const float* __restrict ring, const int length, int pos,
							 const float k, const int n, const bool avx2 )
	{
		pos %= length;
		auto	done = 0;
		while ( done < n )
		{
			const auto	run = std::min ( n - done, length - pos );
			firSet ( out + done, ring + pos, k, run, avx2 );
			done += run;
			pos = 0;
		}
	}

	// out[ 0..n ) += k * ring[ ( pos + i ) % length ]
	inline void ringTapAcc ( float* __restrict out, const float* __restrict ring, const int length, int pos,
							 const float k, const int n, const bool avx2 )
	{
		pos %= length;
		auto	done = 0;
		while ( done < n )
		{
			const auto	run = std::min ( n - done, length - pos );
			firAcc ( out + done, ring + pos, k, run, avx2 );
			done += run;
			pos = 0;
		}
	}

	// ring[ ( pos + i ) % length ] = src[ 0..n )
	inline void ringWrite ( float* __restrict ring, const int length, int pos, const float* __restrict src, const int n )
	{
		pos %= length;
		auto	done = 0;
		while ( done < n )
		{
			const auto	run = std::min ( n - done, length - pos );
			memcpy ( ring + pos, src + done, run * sizeof ( float ) );
			done += run;
			pos = 0;
		}
	}

	// out[ 0..n ) = ring[ ( pos + i ) % length ]
	inline void ringCopyOut ( float* __restrict out, const float* __restrict ring, const int length, int pos, const int n )
	{
		pos %= length;
		auto	done = 0;
		while ( done < n )
		{
			const auto	run = std::min ( n - done, length - pos );
			memcpy ( out + done, ring + pos, run * sizeof ( float ) );
			done += run;
			pos = 0;
		}
	}

	// in-place allpass over a ring buffer (see allpassRunScalar for the math)
	inline void allpassRing ( float* __restrict ring, const int length, int pos, float* __restrict x, const float g, const int n, const bool avx2 )
	{
		pos %= length;
		auto	done = 0;
		while ( done < n )
		{
			const auto	run = std::min ( n - done, length - pos );
			allpassRun ( ring + pos, x + done, g, run, avx2 );
			done += run;
			pos = 0;
		}
	}

	// same, with a per-sample feedback array
	inline void allpassRingV ( float* __restrict ring, const int length, int pos, float* __restrict x, const float* __restrict g, const int n, const bool avx2 )
	{
		pos %= length;
		auto	done = 0;
		while ( done < n )
		{
			const auto	run = std::min ( n - done, length - pos );
			allpassRunV ( ring + pos, x + done, g + done, run, avx2 );
			done += run;
			pos = 0;
		}
	}
}
//-----------------------------------------------------------------------------

//forward declaration
template<typename T, int maxLength> class Allpass;
template<typename T, int maxLength> class StaticAllpassFourTap;
template<typename T, int maxLength> class StaticDelayLine;
template<typename T, int maxLength> class StaticDelayLineFourTap;
template<typename T, int maxLength> class StaticDelayLineEightTap;
template<typename T, int OverSampleCount> class StateVariable;

template<typename T>
class MVerb
{
private:
    Allpass<T, 600> allpass[4] = {};
    StaticAllpassFourTap<T, 4000> allpassFourTap[4] = {};
    StateVariable<T,4> bandwidthFilter[2] = {};
    StateVariable<T,4> damping[2] = {};
    StaticDelayLine<T, 44100> predelay = {};
    StaticDelayLineFourTap<T, 7000> staticDelayLine[4] = {};
    StaticDelayLineEightTap<T, 4000> earlyReflectionsDelayLine[2] = {};
    T SampleRate = {};
    T DampingFreq = {};
    T Density1 = {};
    T Density2 = {};
    T BandwidthFreq = {};
    T PreDelayTime = {};
    T Decay = {};
    T Gain = {};
    T Mix = {};
    T EarlyMix = {};
    T Size = {};

    T MixSmooth = {};
    T EarlyLateSmooth = {};
    T BandwidthSmooth = {};
    T DampingSmooth = {};
    T PredelaySmooth = {};
    T SizeSmooth = {};
    T DensitySmooth = {};
    T DecaySmooth = {};

    T PreviousLeftTank = {};
    T PreviousRightTank = {};

    int ControlRate = 0;
    int ControlRateCounter = 0;

    // block processing: chunk size, and the largest safe chunk given the tap
    // layout (see computeChunkLimit); below kMinChunk the scalar path runs
    static constexpr int kChunk = 128;
    static constexpr int kMinChunk = 8;
    int chunkLimit = 0;
#if MVERB_SSE
    bool useAVX2 = mverb_kernels::hasAVX2 ();
#else
    static constexpr bool useAVX2 = false;
#endif

public:
    enum
		{
			DAMPINGFREQ=0,
			DENSITY,
			BANDWIDTHFREQ,
            DECAY,
            PREDELAY,
            SIZE,
            GAIN,
            MIX,
            EARLYMIX,
            NUM_PARAMS
		};

    MVerb(){
        DampingFreq = 0.9f;
        BandwidthFreq = 0.9f;
        SampleRate = 44100.;
        Decay = 0.5;
        Gain = 1.;
        Mix = 1.;
        Size = 1.;
        EarlyMix = 1.;
        PreviousLeftTank = 0.;
        PreviousRightTank = 0.;
        PreDelayTime = 100 * (SampleRate / 1000);
        MixSmooth = EarlyLateSmooth = BandwidthSmooth = DampingSmooth = PredelaySmooth = SizeSmooth = DecaySmooth = DensitySmooth = 0.;
        ControlRate = static_cast<int>(SampleRate / 1000);
        ControlRateCounter = 0;
        reset();
    }

    ~MVerb(){
        //nowt to do here
    }

    void process(T **inputs, T **outputs, int sampleFrames){
        if (sampleFrames <= 0)
            return;
        if constexpr ( std::is_same_v<T, float> )
        {
            if ( chunkLimit >= kMinChunk )
            {
                processBlock ( inputs, outputs, sampleFrames );
                return;
            }
        }
        processScalar ( inputs, outputs, sampleFrames );
    }

private:
    // original per-sample implementation, kept as reference and as fallback for
    // non-float instantiations / degenerate configurations
    void processScalar(T **inputs, T **outputs, int sampleFrames){
        T OneOverSampleFrames = static_cast<T>(1. / sampleFrames);
        T MixDelta	= (Mix - MixSmooth) * OneOverSampleFrames;
        T EarlyLateDelta = (EarlyMix - EarlyLateSmooth) * OneOverSampleFrames;
        T BandwidthDelta = static_cast<T>((((BandwidthFreq * 18400.) + 100.) - BandwidthSmooth) * OneOverSampleFrames);
        T DampingDelta = static_cast<T>((((DampingFreq * 18400.) + 100.) - DampingSmooth) * OneOverSampleFrames);
        T PredelayDelta = static_cast<T>(((PreDelayTime * 200 * (SampleRate / 1000)) - PredelaySmooth) * OneOverSampleFrames);
        T SizeDelta	= static_cast<T>((Size - SizeSmooth) * OneOverSampleFrames);
        T DecayDelta = static_cast<T>((((0.7995f * Decay) + 0.005) - DecaySmooth) * OneOverSampleFrames);
        T DensityDelta = static_cast<T>((((0.7995f * Density1) + 0.005) - DensitySmooth) * OneOverSampleFrames);
        for(int i=0;i<sampleFrames;++i){
            T left = inputs[0][i];
            T right = inputs[1][i];
            MixSmooth += MixDelta;
            EarlyLateSmooth += EarlyLateDelta;
            BandwidthSmooth += BandwidthDelta;
            DampingSmooth += DampingDelta;
            PredelaySmooth += PredelayDelta;
            SizeSmooth += SizeDelta;
            DecaySmooth += DecayDelta;
            DensitySmooth += DensityDelta;
            if (ControlRateCounter >= ControlRate){
                ControlRateCounter = 0;
                bandwidthFilter[0].Frequency(BandwidthSmooth);
                bandwidthFilter[1].Frequency(BandwidthSmooth);
                damping[0].Frequency(DampingSmooth);
                damping[1].Frequency(DampingSmooth);
            }
            ++ControlRateCounter;
            predelay.SetLength(static_cast<int>(PredelaySmooth));
            Density2 = static_cast<T>(DecaySmooth + 0.15);
            if (Density2 > 0.5)
                Density2 = 0.5;
            if (Density2 < 0.25)
                Density2 = 0.25;
            allpassFourTap[1].SetFeedback(Density2);
            allpassFourTap[3].SetFeedback(Density2);
            allpassFourTap[0].SetFeedback(Density1);
            allpassFourTap[2].SetFeedback(Density1);
            T bandwidthLeft = bandwidthFilter[0](left) ;
            T bandwidthRight = bandwidthFilter[1](right) ;
            T earlyReflectionsL = static_cast<T>(earlyReflectionsDelayLine[0] ( bandwidthLeft * 0.5 + bandwidthRight * 0.3 )
                                + earlyReflectionsDelayLine[0].GetIndex(2) * 0.6
                                + earlyReflectionsDelayLine[0].GetIndex(3) * 0.4
                                + earlyReflectionsDelayLine[0].GetIndex(4) * 0.3
                                + earlyReflectionsDelayLine[0].GetIndex(5) * 0.3
                                + earlyReflectionsDelayLine[0].GetIndex(6) * 0.1
                                + earlyReflectionsDelayLine[0].GetIndex(7) * 0.1
                                + ( bandwidthLeft * 0.4 + bandwidthRight * 0.2 ) * 0.5);
            T earlyReflectionsR = static_cast<T>(earlyReflectionsDelayLine[1] ( bandwidthLeft * 0.3 + bandwidthRight * 0.5 )
                                + earlyReflectionsDelayLine[1].GetIndex(2) * 0.6
                                + earlyReflectionsDelayLine[1].GetIndex(3) * 0.4
                                + earlyReflectionsDelayLine[1].GetIndex(4) * 0.3
                                + earlyReflectionsDelayLine[1].GetIndex(5) * 0.3
                                + earlyReflectionsDelayLine[1].GetIndex(6) * 0.1
                                + earlyReflectionsDelayLine[1].GetIndex(7) * 0.1
                                + ( bandwidthLeft * 0.2 + bandwidthRight * 0.4 ) * 0.5);
            T predelayMonoInput = predelay(( bandwidthRight + bandwidthLeft ) * 0.5f);
            T smearedInput = predelayMonoInput;
            for(int j=0;j<4;j++)
                smearedInput = allpass[j] ( smearedInput );
            T leftTank = allpassFourTap[0] ( smearedInput + PreviousRightTank ) ;
            leftTank = staticDelayLine[0] (leftTank);
            leftTank = damping[0](leftTank);
            leftTank = allpassFourTap[1](leftTank);
            leftTank = staticDelayLine[1](leftTank);
            T rightTank = allpassFourTap[2] (smearedInput + PreviousLeftTank) ;
            rightTank = staticDelayLine[2](rightTank);
            rightTank = damping[1] (rightTank);
            rightTank = allpassFourTap[3](rightTank);
            rightTank = staticDelayLine[3](rightTank);
            PreviousLeftTank = leftTank * DecaySmooth;
            PreviousRightTank = rightTank * DecaySmooth;
            T accumulatorL = static_cast<T>((0.6*staticDelayLine[2].GetIndex(1))
                            +(0.6*staticDelayLine[2].GetIndex(2))
                            -(0.6*allpassFourTap[3].GetIndex(1))
                            +(0.6*staticDelayLine[3].GetIndex(1))
                            -(0.6*staticDelayLine[0].GetIndex(1))
                            -(0.6*allpassFourTap[1].GetIndex(1))
                            -(0.6*staticDelayLine[1].GetIndex(1)));
            T accumulatorR = static_cast<T>((0.6*staticDelayLine[0].GetIndex(2))
                            +(0.6*staticDelayLine[0].GetIndex(3))
                            -(0.6*allpassFourTap[1].GetIndex(2))
                            +(0.6*staticDelayLine[1].GetIndex(2))
                            -(0.6*staticDelayLine[2].GetIndex(3))
                            -(0.6*allpassFourTap[3].GetIndex(2))
                            -(0.6*staticDelayLine[3].GetIndex(2)));
            accumulatorL = ((accumulatorL * EarlyMix) + ((1 - EarlyMix) * earlyReflectionsL));
            accumulatorR = ((accumulatorR * EarlyMix) + ((1 - EarlyMix) * earlyReflectionsR));
            left = ( left + MixSmooth * ( accumulatorL - left ) ) * Gain;
            right = ( right + MixSmooth * ( accumulatorR - right ) ) * Gain;
            outputs[0][i] = left;
            outputs[1][i] = right;
        }
    }

    // Block-based processing: identical signal routing to processScalar, but
    // everything runs as vectorized passes over small chunks, including the
    // feedback tank, whose per-chunk feedback vector is known up front (see the
    // tank section); only the two damping filters keep a per-sample recurrence.
    // Chunks never exceed chunkLimit, which (together with reading each output
    // tap before or after the tank writes, see the output-tap comments)
    // guarantees every block-read tap sees exactly the values the per-sample
    // order would read. Deviations from processScalar: float rounding (float vs
    // double accumulation, folded SVF, fma) and the predelay length updating at
    // chunk rate instead of per sample, both far below audibility.
    void processBlock(T **inputs, T **outputs, int sampleFrames){
        static_assert ( std::is_same_v<T, float>, "the SIMD block path is float-only" );
        using namespace mverb_kernels;

        const T OneOverSampleFrames = static_cast<T>(1. / sampleFrames);
        const T MixDelta	= (Mix - MixSmooth) * OneOverSampleFrames;
        const T EarlyLateDelta = (EarlyMix - EarlyLateSmooth) * OneOverSampleFrames;
        const T BandwidthDelta = static_cast<T>((((BandwidthFreq * 18400.) + 100.) - BandwidthSmooth) * OneOverSampleFrames);
        const T DampingDelta = static_cast<T>((((DampingFreq * 18400.) + 100.) - DampingSmooth) * OneOverSampleFrames);
        const T PredelayDelta = static_cast<T>(((PreDelayTime * 200 * (SampleRate / 1000)) - PredelaySmooth) * OneOverSampleFrames);
        const T SizeDelta	= static_cast<T>((Size - SizeSmooth) * OneOverSampleFrames);
        const T DecayDelta = static_cast<T>((((0.7995f * Decay) + 0.005) - DecaySmooth) * OneOverSampleFrames);
        const T DensityDelta = static_cast<T>((((0.7995f * Density1) + 0.005) - DensitySmooth) * OneOverSampleFrames);

        alignas ( 32 ) float	bwL[ kChunk ], bwR[ kChunk ];
        alignas ( 32 ) float	erL[ kChunk ], erR[ kChunk ];
        alignas ( 32 ) float	accL[ kChunk ], accR[ kChunk ];
        alignas ( 32 ) float	tmp[ kChunk ], sm[ kChunk ];
        alignas ( 32 ) float	tnkL[ kChunk ], tnkR[ kChunk ];
        alignas ( 32 ) float	dmpL[ kChunk ], dmpR[ kChunk ];
        alignas ( 32 ) float	ltOut[ kChunk ], rtOut[ kChunk ];
        alignas ( 32 ) float	fbFromL[ kChunk ], fbFromR[ kChunk ];

        const T	earlyMix = EarlyMix;
        const T	lateMix = 1 - EarlyMix;

        auto	done = 0;
        while ( done < sampleFrames )
        {
            // filter coefficients update at chunk rate (the scalar path keeps the
            // original every-SampleRate/1000-samples tick; the timing difference
            // only matters while a smoothed parameter is still ramping, a brief,
            // inaudible transient). Frequency() early-outs on unchanged values,
            // so this costs nothing once the smoothed parameters have settled.
            {
                const T	bwFreq = BandwidthSmooth + BandwidthDelta;
                const T	dmpFreq = DampingSmooth + DampingDelta;
                bandwidthFilter[0].Frequency ( bwFreq );
                bandwidthFilter[1].Frequency ( bwFreq );
                damping[0].Frequency ( dmpFreq );
                damping[1].Frequency ( dmpFreq );
            }

            const auto	n = std::min ( { kChunk, chunkLimit, sampleFrames - done } );

            const float*	inL = inputs[ 0 ] + done;
            const float*	inR = inputs[ 1 ] + done;

            // --- input bandwidth filters (folded SVF, one affine step per sample)
            for ( auto i = 0; i < n; ++i )
            {
                bwL[ i ] = bandwidthFilter[0].Tick ( inL[ i ] );
                bwR[ i ] = bandwidthFilter[1].Tick ( inR[ i ] );
            }

            // --- early reflections: per side 7 delay taps plus a direct feed.
            // All taps are at least chunkLimit samples old, so read them as block
            // FIRs before writing this chunk's input into the lines.
            {
                auto&	e0 = earlyReflectionsDelayLine[ 0 ];
                auto&	e1 = earlyReflectionsDelayLine[ 1 ];

                // operator() return value == the tap at the write head (Length samples old)
                ringTapSet ( erL, e0.Buffer (), e0.GetLength (), e0.TapPos ( 1 ), 1.0f, n, useAVX2 );
                ringTapSet ( erR, e1.Buffer (), e1.GetLength (), e1.TapPos ( 1 ), 1.0f, n, useAVX2 );

                // GetIndex(t) reads index(t+1) *after* the per-sample increment, hence pos + 1
                static constexpr float	erCoeff[ 6 ] = { 0.6f, 0.4f, 0.3f, 0.3f, 0.1f, 0.1f };
                for ( auto t = 0; t < 6; ++t )
                {
                    ringTapAcc ( erL, e0.Buffer (), e0.GetLength (), e0.TapPos ( t + 3 ) + 1, erCoeff[ t ], n, useAVX2 );
                    ringTapAcc ( erR, e1.Buffer (), e1.GetLength (), e1.TapPos ( t + 3 ) + 1, erCoeff[ t ], n, useAVX2 );
                }

                // direct feeds: ( bwL*0.4 + bwR*0.2 ) * 0.5 resp. ( bwL*0.2 + bwR*0.4 ) * 0.5
                firAcc ( erL, bwL, 0.2f, n, useAVX2 );
                firAcc ( erL, bwR, 0.1f, n, useAVX2 );
                firAcc ( erR, bwL, 0.1f, n, useAVX2 );
                firAcc ( erR, bwR, 0.2f, n, useAVX2 );

                // line inputs: bwL*0.5 + bwR*0.3 resp. bwL*0.3 + bwR*0.5
                firSet ( tmp, bwL, 0.5f, n, useAVX2 );
                firAcc ( tmp, bwR, 0.3f, n, useAVX2 );
                ringWrite ( e0.Buffer (), e0.GetLength (), e0.TapPos ( 1 ), tmp, n );
                firSet ( tmp, bwL, 0.3f, n, useAVX2 );
                firAcc ( tmp, bwR, 0.5f, n, useAVX2 );
                ringWrite ( e1.Buffer (), e1.GetLength (), e1.TapPos ( 1 ), tmp, n );

                e0.Advance ( n );
                e1.Advance ( n );
            }

            // --- output taps, part 1. Read positions never collide with this
            // chunk's tank writes except in two cases, decided by the tap's
            // distance d ahead of the write head: a tap with 0 < d < n sits on
            // the oldest slots of the line, which the tank loop overwrites
            // before a read-after-the-loop would see them, the per-sample
            // order reads the OLD data there, so those taps are read before
            // the loop. A tap at the head itself (d == 0) or further ahead
            // (d >= n) must see this chunk's writes resp. is untouched, both
            // are read after the loop.
            struct Tap { const float* buf; int len; int wr; int pos; float k; float* dst; };
            const Tap accTaps[ 14 ] = {
                { staticDelayLine[2].Buffer (), staticDelayLine[2].GetLength (), staticDelayLine[2].TapPos ( 1 ), staticDelayLine[2].TapPos ( 2 ),  0.6f, accL },
                { staticDelayLine[2].Buffer (), staticDelayLine[2].GetLength (), staticDelayLine[2].TapPos ( 1 ), staticDelayLine[2].TapPos ( 3 ),  0.6f, accL },
                { allpassFourTap[3].Buffer (),  allpassFourTap[3].GetLength (),  allpassFourTap[3].TapPos ( 1 ),  allpassFourTap[3].TapPos ( 2 ),  -0.6f, accL },
                { staticDelayLine[3].Buffer (), staticDelayLine[3].GetLength (), staticDelayLine[3].TapPos ( 1 ), staticDelayLine[3].TapPos ( 2 ),  0.6f, accL },
                { staticDelayLine[0].Buffer (), staticDelayLine[0].GetLength (), staticDelayLine[0].TapPos ( 1 ), staticDelayLine[0].TapPos ( 2 ), -0.6f, accL },
                { allpassFourTap[1].Buffer (),  allpassFourTap[1].GetLength (),  allpassFourTap[1].TapPos ( 1 ),  allpassFourTap[1].TapPos ( 2 ),  -0.6f, accL },
                { staticDelayLine[1].Buffer (), staticDelayLine[1].GetLength (), staticDelayLine[1].TapPos ( 1 ), staticDelayLine[1].TapPos ( 2 ), -0.6f, accL },
                { staticDelayLine[0].Buffer (), staticDelayLine[0].GetLength (), staticDelayLine[0].TapPos ( 1 ), staticDelayLine[0].TapPos ( 3 ),  0.6f, accR },
                { staticDelayLine[0].Buffer (), staticDelayLine[0].GetLength (), staticDelayLine[0].TapPos ( 1 ), staticDelayLine[0].TapPos ( 4 ),  0.6f, accR },
                { allpassFourTap[1].Buffer (),  allpassFourTap[1].GetLength (),  allpassFourTap[1].TapPos ( 1 ),  allpassFourTap[1].TapPos ( 3 ),  -0.6f, accR },
                { staticDelayLine[1].Buffer (), staticDelayLine[1].GetLength (), staticDelayLine[1].TapPos ( 1 ), staticDelayLine[1].TapPos ( 3 ),  0.6f, accR },
                { staticDelayLine[2].Buffer (), staticDelayLine[2].GetLength (), staticDelayLine[2].TapPos ( 1 ), staticDelayLine[2].TapPos ( 4 ), -0.6f, accR },
                { allpassFourTap[3].Buffer (),  allpassFourTap[3].GetLength (),  allpassFourTap[3].TapPos ( 1 ),  allpassFourTap[3].TapPos ( 3 ),  -0.6f, accR },
                { staticDelayLine[3].Buffer (), staticDelayLine[3].GetLength (), staticDelayLine[3].TapPos ( 1 ), staticDelayLine[3].TapPos ( 3 ), -0.6f, accR },
            };
            bool tapPre[ 14 ];
            memset ( accL, 0, n * sizeof ( float ) );
            memset ( accR, 0, n * sizeof ( float ) );
            for ( auto t = 0; t < 14; ++t )
            {
                const auto&	tap = accTaps[ t ];
                const auto	d = ( ( tap.pos + 1 - tap.wr ) % tap.len + tap.len ) % tap.len;
                tapPre[ t ] = ( d != 0 && d < n );
                if ( tapPre[ t ] )
                    ringTapAcc ( tap.dst, tap.buf, tap.len, tap.pos + 1, tap.k, n, useAVX2 );
            }

            // --- feedback tank as block passes. The tank looks sample-serial
            // (PreviousLeft/RightTank cross-feedback), but each chain ends in a
            // static delay line whose output is purely old data (>= chunkLimit
            // samples old), so the whole chunk's feedback vector is known up
            // front and every stage becomes a vector pass over the chunk; only
            // the damping filters keep a true per-sample recurrence.
            {
                // decay / density ramps (tiny scalar prepass)
                alignas ( 32 ) float	dcArr[ kChunk ], d2Arr[ kChunk ];
                auto	dc = DecaySmooth;
                for ( auto i = 0; i < n; ++i )
                {
                    dc += DecayDelta;
                    dcArr[ i ] = dc;
                    auto	d2 = static_cast<T>( dc + 0.15 );
                    if ( d2 > 0.5 )
                        d2 = 0.5;
                    if ( d2 < 0.25 )
                        d2 = 0.25;
                    d2Arr[ i ] = d2;
                }
                DecaySmooth = dc;
                Density2 = d2Arr[ n - 1 ];	// member parity with the scalar path

                auto&	ap0 = allpassFourTap[ 0 ];
                auto&	ap1 = allpassFourTap[ 1 ];
                auto&	ap2 = allpassFourTap[ 2 ];
                auto&	ap3 = allpassFourTap[ 3 ];
                auto&	sd0 = staticDelayLine[ 0 ];
                auto&	sd1 = staticDelayLine[ 1 ];
                auto&	sd2 = staticDelayLine[ 2 ];
                auto&	sd3 = staticDelayLine[ 3 ];

                // this chunk's tank outputs already sit in sd1/sd3, read them
                // up front and build the cross-feedback vectors, shifted by one
                // sample (first entry carried over from the previous chunk)
                ringCopyOut ( ltOut, sd1.Buffer (), sd1.GetLength (), sd1.TapPos ( 1 ), n );
                ringCopyOut ( rtOut, sd3.Buffer (), sd3.GetLength (), sd3.TapPos ( 1 ), n );
                fbFromL[ 0 ] = PreviousLeftTank;
                fbFromR[ 0 ] = PreviousRightTank;
                mulV ( fbFromL + 1, ltOut, dcArr, n - 1, useAVX2 );
                mulV ( fbFromR + 1, rtOut, dcArr, n - 1, useAVX2 );
                PreviousLeftTank = ltOut[ n - 1 ] * dcArr[ n - 1 ];
                PreviousRightTank = rtOut[ n - 1 ] * dcArr[ n - 1 ];

                // predelayed mono input through the four smearing allpasses.
                // The predelay stays per-sample: its length ramps with
                // PredelaySmooth, and keeping the exact per-sample stepping is
                // cheap while avoiding a parameter-change transient vs the
                // scalar path.
                firSet ( tmp, bwL, 0.5f, n, useAVX2 );
                firAcc ( tmp, bwR, 0.5f, n, useAVX2 );
                auto	pd = PredelaySmooth;
                for ( auto i = 0; i < n; ++i )
                {
                    pd += PredelayDelta;
                    predelay.SetLength ( static_cast<int>( pd ) );
                    sm[ i ] = predelay ( tmp[ i ] );
                }
                PredelaySmooth = pd;
                for ( auto j = 0; j < 4; ++j )
                {
                    auto&	ap = allpass[ j ];
                    allpassRing ( ap.Buffer (), ap.GetLength (), ap.Index (), sm, ap.GetFeedback (), n, useAVX2 );
                    ap.Advance ( n );
                }

                // both chains up to the damping filters (left is fed the RIGHT
                // tank's previous output and vice versa, as in the scalar path)
                addV ( tnkL, sm, fbFromR, n, useAVX2 );
                addV ( tnkR, sm, fbFromL, n, useAVX2 );
                allpassRing ( ap0.Buffer (), ap0.GetLength (), ap0.TapPos ( 1 ), tnkL, Density1, n, useAVX2 );
                allpassRing ( ap2.Buffer (), ap2.GetLength (), ap2.TapPos ( 1 ), tnkR, Density1, n, useAVX2 );
                ringCopyOut ( dmpL, sd0.Buffer (), sd0.GetLength (), sd0.TapPos ( 1 ), n );
                ringWrite ( sd0.Buffer (), sd0.GetLength (), sd0.TapPos ( 1 ), tnkL, n );
                ringCopyOut ( dmpR, sd2.Buffer (), sd2.GetLength (), sd2.TapPos ( 1 ), n );
                ringWrite ( sd2.Buffer (), sd2.GetLength (), sd2.TapPos ( 1 ), tnkR, n );

                // damping, the only remaining per-sample recurrence
                for ( auto i = 0; i < n; ++i )
                {
                    dmpL[ i ] = damping[ 0 ].Tick ( dmpL[ i ] );
                    dmpR[ i ] = damping[ 1 ].Tick ( dmpR[ i ] );
                }

                // rest of the chains; density2 ramps per sample via d2Arr
                allpassRingV ( ap1.Buffer (), ap1.GetLength (), ap1.TapPos ( 1 ), dmpL, d2Arr, n, useAVX2 );
                allpassRingV ( ap3.Buffer (), ap3.GetLength (), ap3.TapPos ( 1 ), dmpR, d2Arr, n, useAVX2 );
                ringWrite ( sd1.Buffer (), sd1.GetLength (), sd1.TapPos ( 1 ), dmpL, n );
                ringWrite ( sd3.Buffer (), sd3.GetLength (), sd3.TapPos ( 1 ), dmpR, n );
            }

            // --- output taps, part 2: everything not read before the loop
            for ( auto t = 0; t < 14; ++t )
            {
                if ( !tapPre[ t ] )
                {
                    const auto&	tap = accTaps[ t ];
                    ringTapAcc ( tap.dst, tap.buf, tap.len, tap.pos + 1, tap.k, n, useAVX2 );
                }
            }
            for ( auto k = 0; k < 4; ++k )
            {
                allpassFourTap[ k ].Advance ( n );
                staticDelayLine[ k ].Advance ( n );
            }

            // --- final mix (MixSmooth ramps per sample, EarlyMix is used unsmoothed
            // just like in the scalar path)
            mixOut ( outputs[ 0 ] + done, inL, accL, erL, earlyMix, lateMix, MixSmooth, MixDelta, Gain, n, useAVX2 );
            mixOut ( outputs[ 1 ] + done, inR, accR, erR, earlyMix, lateMix, MixSmooth, MixDelta, Gain, n, useAVX2 );

            // advance the remaining smoothed parameters to the end of the chunk
            // (PredelaySmooth / DecaySmooth were advanced in the tank section)
            MixSmooth += static_cast<T>( n ) * MixDelta;
            EarlyLateSmooth += static_cast<T>( n ) * EarlyLateDelta;
            BandwidthSmooth += static_cast<T>( n ) * BandwidthDelta;
            DampingSmooth += static_cast<T>( n ) * DampingDelta;
            SizeSmooth += static_cast<T>( n ) * SizeDelta;
            DensitySmooth += static_cast<T>( n ) * DensityDelta;

            done += n;	// ControlRateCounter is only used by the scalar path
        }
    }

    // Largest chunk the block-read phases may process at once: the age (in
    // samples) of the youngest audio any block-read tap looks at. The index
    // offsets only change on reset() / SIZE changes, so it is recomputed there.
    void computeChunkLimit ()
    {
        const auto	tapAge = [] ( const int writePos, const int tapPos, const int length ) -> int
        {
            if ( length <= 0 )
                return 0;
            return ( ( writePos - tapPos - 1 ) % length + length ) % length;
        };

        auto	limit = std::numeric_limits<int>::max ();

        for ( auto e = 0; e < 2; ++e )
        {
            const auto&	line = earlyReflectionsDelayLine[ e ];
            limit = std::min ( limit, line.GetLength () );	// write-head tap, Length samples old
            for ( auto t = 3; t <= 8; ++t )
                limit = std::min ( limit, tapAge ( line.TapPos ( 1 ), line.TapPos ( t ), line.GetLength () ) );
        }

        // output taps: a tap at the write head (d == 0) always reads after the
        // tank loop; one slightly ahead (d < kChunk) is read before the loop
        // and needs age (len - d) >= chunk; one further ahead is read after the
        // loop and needs d >= chunk (trivially true for d >= kChunk, but keeps
        // the bound exact for short lines)
        const auto	fourTapLimits = [ & ] ( const auto& line )
        {
            const auto	len = line.GetLength ();
            if ( len <= 0 )
            {
                limit = 0;
                return;
            }
            for ( auto t = 2; t <= 4; ++t )
            {
                const auto	d = ( ( line.TapPos ( t ) + 1 - line.TapPos ( 1 ) ) % len + len ) % len;
                if ( d != 0 )
                    limit = std::min ( limit, d < kChunk ? len - d : d );
            }
        };
        for ( auto k = 0; k < 4; ++k )
            fourTapLimits ( staticDelayLine[ k ] );
        fourTapLimits ( allpassFourTap[ 1 ] );
        fourTapLimits ( allpassFourTap[ 3 ] );

        // every block-processed line is read at its write head (data Length
        // samples old), so no line may be shorter than a chunk (the predelay
        // is handled dynamically per chunk)
        for ( auto k = 0; k < 4; ++k )
        {
            limit = std::min ( limit, allpass[ k ].GetLength () );
            limit = std::min ( limit, allpassFourTap[ k ].GetLength () );
            limit = std::min ( limit, staticDelayLine[ k ].GetLength () );
        }

        chunkLimit = limit;
    }

public:
    void reset(){
        ControlRateCounter = 0;
        bandwidthFilter[0].SetSampleRate(SampleRate);
        bandwidthFilter[1].SetSampleRate(SampleRate);
        bandwidthFilter[0].Reset();
        bandwidthFilter[1].Reset();
        damping[0].SetSampleRate(SampleRate);
        damping[1].SetSampleRate(SampleRate);
        damping[0].Reset();
        damping[1].Reset();
        predelay.Clear();
        predelay.SetLength(static_cast<int>(PreDelayTime));
        allpass[0].Clear();
        allpass[1].Clear();
        allpass[2].Clear();
        allpass[3].Clear();
        allpass[0].SetLength(static_cast<int>(0.0048 * SampleRate));
        allpass[1].SetLength(static_cast<int>(0.0036 * SampleRate));
        allpass[2].SetLength(static_cast<int>(0.0127 * SampleRate));
        allpass[3].SetLength(static_cast<int>(0.0093 * SampleRate));
        allpass[0].SetFeedback(0.75);
        allpass[1].SetFeedback(0.75);
        allpass[2].SetFeedback(0.625);
        allpass[3].SetFeedback(0.625);
        allpassFourTap[0].Clear();
        allpassFourTap[1].Clear();
        allpassFourTap[2].Clear();
        allpassFourTap[3].Clear();
        allpassFourTap[0].SetLength(static_cast<int>(0.020 * SampleRate * Size));
        allpassFourTap[1].SetLength(static_cast<int>(0.060 * SampleRate * Size));
        allpassFourTap[2].SetLength(static_cast<int>(0.030 * SampleRate * Size));
        allpassFourTap[3].SetLength(static_cast<int>(0.089 * SampleRate * Size));
        allpassFourTap[0].SetFeedback(Density1);
        allpassFourTap[1].SetFeedback(Density2);
        allpassFourTap[2].SetFeedback(Density1);
        allpassFourTap[3].SetFeedback(Density2);
        allpassFourTap[0].SetIndex(0, 0, 0, 0);
        allpassFourTap[1].SetIndex(0, static_cast<int>(0.006 * SampleRate * Size), static_cast<int>(0.041 * SampleRate * Size), 0);
        allpassFourTap[2].SetIndex(0, 0, 0, 0);
        allpassFourTap[3].SetIndex(0, static_cast<int>(0.031 * SampleRate * Size), static_cast<int>(0.011 * SampleRate * Size), 0);
        staticDelayLine[0].Clear();
        staticDelayLine[1].Clear();
        staticDelayLine[2].Clear();
        staticDelayLine[3].Clear();
        staticDelayLine[0].SetLength(static_cast<int>(0.15 * SampleRate * Size));
        staticDelayLine[1].SetLength(static_cast<int>(0.12 * SampleRate * Size));
        staticDelayLine[2].SetLength(static_cast<int>(0.14 * SampleRate * Size));
        staticDelayLine[3].SetLength(static_cast<int>(0.11 * SampleRate * Size));
        staticDelayLine[0].SetIndex(0, static_cast<int>(0.067 * SampleRate * Size), static_cast<int>(0.011 * SampleRate * Size), static_cast<int>(0.121 * SampleRate * Size));
        staticDelayLine[1].SetIndex(0, static_cast<int>(0.036 * SampleRate * Size), static_cast<int>(0.089 * SampleRate * Size), 0);
        staticDelayLine[2].SetIndex(0, static_cast<int>(0.0089 * SampleRate * Size), static_cast<int>(0.099 * SampleRate * Size), 0);
        staticDelayLine[3].SetIndex(0, static_cast<int>(0.067 * SampleRate * Size), static_cast<int>(0.0041 * SampleRate * Size), 0);
        earlyReflectionsDelayLine[0].Clear();
        earlyReflectionsDelayLine[1].Clear();
        earlyReflectionsDelayLine[0].SetLength(static_cast<int>(0.089 * SampleRate));
        earlyReflectionsDelayLine[0].SetIndex(0, static_cast<int>(0.0199 * SampleRate), static_cast<int>(0.0219 * SampleRate), static_cast<int>(0.0354 * SampleRate), static_cast<int>(0.0389 * SampleRate), static_cast<int>(0.0414 * SampleRate), static_cast<int>(0.0692 * SampleRate), 0);
        earlyReflectionsDelayLine[1].SetLength(static_cast<int>(0.069 * SampleRate));
        earlyReflectionsDelayLine[1].SetIndex(0, static_cast<int>(0.0099 * SampleRate), static_cast<int>(0.011 * SampleRate), static_cast<int>(0.0182 * SampleRate), static_cast<int>(0.0189 * SampleRate), static_cast<int>(0.0213 * SampleRate), static_cast<int>(0.0431 * SampleRate), 0);
        computeChunkLimit();
    }

    void setParameter(int index, T value){
        switch(index){
            case DAMPINGFREQ:
                    DampingFreq = static_cast<T>(1. - value);
                    break;
            case DENSITY:
                    Density1 = value;
                    break;
            case BANDWIDTHFREQ:
                    BandwidthFreq = value;
                    break;
            case PREDELAY:
                    PreDelayTime = value;
                    break;
            case SIZE:
                    Size = static_cast<T>((0.95 * value) + 0.05);
					allpassFourTap[0].Clear();
					allpassFourTap[1].Clear();
					allpassFourTap[2].Clear();
					allpassFourTap[3].Clear();
                    allpassFourTap[0].SetLength(static_cast<int>(0.020 * SampleRate * Size));
                    allpassFourTap[1].SetLength(static_cast<int>(0.060 * SampleRate * Size));
                    allpassFourTap[2].SetLength(static_cast<int>(0.030 * SampleRate * Size));
                    allpassFourTap[3].SetLength(static_cast<int>(0.089 * SampleRate * Size));
                    allpassFourTap[1].SetIndex(0, static_cast<int>(0.006 * SampleRate * Size), static_cast<int>(0.041 * SampleRate * Size), 0);
                    allpassFourTap[3].SetIndex(0, static_cast<int>(0.031 * SampleRate * Size), static_cast<int>(0.011 * SampleRate * Size), 0);
					staticDelayLine[0].Clear();
					staticDelayLine[1].Clear();
					staticDelayLine[2].Clear();
					staticDelayLine[3].Clear();
                    staticDelayLine[0].SetLength(static_cast<int>(0.15 * SampleRate * Size));
                    staticDelayLine[1].SetLength(static_cast<int>(0.12 * SampleRate * Size));
                    staticDelayLine[2].SetLength(static_cast<int>(0.14 * SampleRate * Size));
                    staticDelayLine[3].SetLength(static_cast<int>(0.11 * SampleRate * Size));
                    staticDelayLine[0].SetIndex(0, static_cast<int>(0.067 * SampleRate * Size), static_cast<int>(0.011 * SampleRate * Size), static_cast<int>(0.121 * SampleRate * Size));
                    staticDelayLine[1].SetIndex(0, static_cast<int>(0.036 * SampleRate * Size), static_cast<int>(0.089 * SampleRate * Size), 0);
                    staticDelayLine[2].SetIndex(0, static_cast<int>(0.0089 * SampleRate * Size), static_cast<int>(0.099 * SampleRate * Size), 0);
                    staticDelayLine[3].SetIndex(0, static_cast<int>(0.067 * SampleRate * Size), static_cast<int>(0.0041 * SampleRate * Size), 0);
                    computeChunkLimit();
                    break;
            case DECAY:
                    Decay = value;
                    break;
            case GAIN:
                    Gain = value;
                    break;
            case MIX:
                    Mix = value;
                    break;
            case EARLYMIX:
                    EarlyMix = value;
                    break;
        }
    }

    float getParameter(int index){
        switch(index){
            case DAMPINGFREQ:
                    return DampingFreq * 100.;
                    break;
            case DENSITY:
                    return Density1 * 100.f;
                    break;
            case BANDWIDTHFREQ:
                    return BandwidthFreq * 100.;
                    break;
            case PREDELAY:
                    return PreDelayTime * 100.;
                    break;
            case SIZE:
                    return (((0.95 * Size) + 0.05)*100.);
                    break;
            case DECAY:
                    return Decay * 100.f;
                    break;
            case GAIN:
                    return Gain * 100.f;
                    break;
            case MIX:
                    return Mix * 100.f;
                    break;
            case EARLYMIX:
                    return EarlyMix * 100.f;
                    break;
            default: return 0.f;
                break;

        }
    }

    void setSampleRate(T sr){
        SampleRate = sr;
        ControlRate = static_cast<int>(SampleRate / 1000);
        reset();
    }
};



template<typename T, int maxLength>
class Allpass
{
private:
    T buffer[maxLength] = {};
    int index = 0;
    int Length = 0;
    T Feedback = {};

public:
    Allpass()
    {
		SetLength ( maxLength - 1 );
		Clear();
		Feedback = 0.5;
    }

	T operator()(T input)
    {
		T output;
		T bufout;
		bufout = buffer[index];
		T temp = input * -Feedback;
		output = bufout + temp;
		buffer[index] = input + ((bufout+temp)*Feedback);
		if(++index>=Length) index = 0;
		return output;

    }

	void SetLength (int Length)
    {
       if( Length >= maxLength )
			Length = maxLength;
	   if( Length < 0 )
			Length = 0;

        this->Length = Length;
    }

	void SetFeedback(T feedback)
    {
        Feedback = feedback;
    }

	// block-processing access (see MVerb::processBlock)
	T* Buffer ()						{ return buffer; }
	int Index () const					{ return index; }
	T GetFeedback () const				{ return Feedback; }
	void Advance ( int n )
	{
		index = Length > 0 ? ( index % Length + n ) % Length : 0;
	}

    void Clear()
    {
        memset(buffer, 0, sizeof(buffer));
		index = 0;
    }

    int GetLength() const
    {
        return Length;
    }
};

template<typename T, int maxLength>
class StaticAllpassFourTap
{
private:
    T buffer[maxLength] = {};
    int index1 = 0;
    int index2 = 0;
    int index3 = 0;
    int index4 = 0;
    int Length = 0;
    T Feedback = {};

public:
    StaticAllpassFourTap()
    {
		SetLength ( maxLength - 1 );
		Clear();
		Feedback = 0.5;
    }

	T operator()(T input)
    {
		T output;
		T bufout;

		bufout = buffer[index1];
		T temp = input * -Feedback;
		output = bufout + temp;
		buffer[index1] = input + ((bufout+temp)*Feedback);

		if(++index1>=Length)
			index1 = 0;
		if(++index2 >= Length)
			index2 = 0;
		if(++index3 >= Length)
			index3 = 0;
		if(++index4 >= Length)
			index4 = 0;

		return output;

    }

	// block-processing access: the write head is processed via Buffer() as a
	// block operation (allpassRing), all indices advance once per chunk
	T* Buffer ()						{ return buffer; }
	const T* Buffer () const			{ return buffer; }

	void Advance (int n)
	{
		if (Length <= 0)
			return;
		index1 = (index1 % Length + n) % Length;
		index2 = (index2 % Length + n) % Length;
		index3 = (index3 % Length + n) % Length;
		index4 = (index4 % Length + n) % Length;
	}

	int TapPos (int tap) const
	{
		switch (tap)
		{
			case 1:  return index1;
			case 2:  return index2;
			case 3:  return index3;
			default: return index4;
		}
	}

	void SetIndex (int Index1, int Index2, int Index3, int Index4)
	{
		index1 = Index1;
		index2 = Index2;
		index3 = Index3;
		index4 = Index4;
	}

	T GetIndex (int Index)
	{
		switch (Index)
		{
			case 0:
				return buffer[index1];
				break;
			case 1:
				return buffer[index2];
				break;
			case 2:
				return buffer[index3];
				break;
			case 3:
				return buffer[index4];
				break;
			default:
				return buffer[index1];
				break;
		}
	}

	void SetLength (int Length)
    {
       if( Length >= maxLength )
			Length = maxLength;
	   if( Length < 0 )
			Length = 0;

        this->Length = Length;
    }


    void Clear()
    {
        memset(buffer, 0, sizeof(buffer));
		index1 = index2  = index3 = index4 = 0;
    }

	void SetFeedback(T feedback)
    {
        Feedback = feedback;
    }


    int GetLength() const
    {
        return Length;
    }
};

template<typename T, int maxLength>
class StaticDelayLine
{
private:
    T buffer[maxLength] = {};
    int index = 0;
    int Length = 0;
    T Feedback = {};

public:
    StaticDelayLine()
    {
		SetLength ( maxLength - 1 );
		Clear();
    }

	T operator()(T input)
    {
		T output = buffer[index];
		buffer[index++] = input;
		if(index >= Length)
			index = 0;
		return output;

    }

	void SetLength (int Length)
    {
       if( Length >= maxLength )
			Length = maxLength;
	   if( Length < 0 )
			Length = 0;

        this->Length = Length;
    }

	// block-processing access (see MVerb::processBlock)
	T* Buffer ()						{ return buffer; }
	int Index () const					{ return index; }
	void Advance ( int n )
	{
		index = Length > 0 ? ( index % Length + n ) % Length : 0;
	}

    void Clear()
    {
        memset(buffer, 0, sizeof(buffer));
		index = 0;
    }

    int GetLength() const
    {
        return Length;
    }
};

template<typename T, int maxLength>
class StaticDelayLineFourTap
{
private:
    T buffer[maxLength] = {};
    int index1 = 0;
    int index2 = 0;
    int index3 = 0;
    int index4 = 0;
    int Length = 0;
    T Feedback = {};

public:
    StaticDelayLineFourTap()
    {
		SetLength ( maxLength - 1 );
		Clear();
    }

	//get ouput and iterate
	T operator()(T input)
    {
		T output = buffer[index1];
		buffer[index1++] = input;
		if(index1 >= Length)
			index1 = 0;
		if(++index2 >= Length)
			index2 = 0;
		if(++index3 >= Length)
			index3 = 0;
		if(++index4 >= Length)
			index4 = 0;
		return output;

    }

	// block-processing access: reads/writes at the write head run as block
	// operations on Buffer(), all indices advance once per chunk
	T* Buffer ()						{ return buffer; }
	const T* Buffer () const			{ return buffer; }

	void Advance (int n)
	{
		if (Length <= 0)
			return;
		index1 = (index1 % Length + n) % Length;
		index2 = (index2 % Length + n) % Length;
		index3 = (index3 % Length + n) % Length;
		index4 = (index4 % Length + n) % Length;
	}

	int TapPos (int tap) const
	{
		switch (tap)
		{
			case 1:  return index1;
			case 2:  return index2;
			case 3:  return index3;
			default: return index4;
		}
	}

	void SetIndex (int Index1, int Index2, int Index3, int Index4)
	{
		index1 = Index1;
		index2 = Index2;
		index3 = Index3;
		index4 = Index4;
	}


	T GetIndex (int Index)
	{
		switch (Index)
		{
			case 0:
				return buffer[index1];
				break;
			case 1:
				return buffer[index2];
				break;
			case 2:
				return buffer[index3];
				break;
			case 3:
				return buffer[index4];
				break;
			default:
				return buffer[index1];
				break;
		}
	}


	void SetLength (int Length)
    {
       if( Length >= maxLength )
			Length = maxLength;
	   if( Length < 0 )
			Length = 0;

        this->Length = Length;
    }


    void Clear()
    {
        memset(buffer, 0, sizeof(buffer));
		index1 = index2  = index3 = index4 = 0;
    }


    int GetLength() const
    {
        return Length;
    }
};

template<typename T, int maxLength>
class StaticDelayLineEightTap
{
private:
    T buffer[maxLength] = {};
    int index1 = 0;
    int index2 = 0;
    int index3 = 0;
    int index4 = 0;
    int index5 = 0;
    int index6 = 0;
    int index7 = 0;
    int index8 = 0;
    int Length = 0;
    T Feedback = {};

public:
    StaticDelayLineEightTap()
    {
		SetLength ( maxLength - 1 );
		Clear();
    }

	//get ouput and iterate
	T operator()(T input)
    {
		T output = buffer[index1];
		buffer[index1++] = input;
		if(index1 >= Length)
			index1 = 0;
		if(++index2 >= Length)
			index2 = 0;
		if(++index3 >= Length)
			index3 = 0;
		if(++index4 >= Length)
			index4 = 0;
		if(++index5 >= Length)
			index5 = 0;
		if(++index6 >= Length)
			index6 = 0;
		if(++index7 >= Length)
			index7 = 0;
		if(++index8 >= Length)
			index8 = 0;
		return output;

    }

	// for block processing: the writes and tap reads run as block operations
	// on Buffer(), all indices advance once per chunk
	void Advance (int n)
	{
		if (Length <= 0)
			return;
		index1 = (index1 % Length + n) % Length;
		index2 = (index2 % Length + n) % Length;
		index3 = (index3 % Length + n) % Length;
		index4 = (index4 % Length + n) % Length;
		index5 = (index5 % Length + n) % Length;
		index6 = (index6 % Length + n) % Length;
		index7 = (index7 % Length + n) % Length;
		index8 = (index8 % Length + n) % Length;
	}

	T* Buffer ()
	{
		return buffer;
	}

	const T* Buffer () const
	{
		return buffer;
	}

	int TapPos (int tap) const
	{
		switch (tap)
		{
			case 1:  return index1;
			case 2:  return index2;
			case 3:  return index3;
			case 4:  return index4;
			case 5:  return index5;
			case 6:  return index6;
			case 7:  return index7;
			default: return index8;
		}
	}

	void SetIndex (int Index1, int Index2, int Index3, int Index4, int Index5, int Index6, int Index7, int Index8)
	{
		index1 = Index1;
		index2 = Index2;
		index3 = Index3;
		index4 = Index4;
		index5 = Index5;
		index6 = Index6;
		index7 = Index7;
		index8 = Index8;
	}


	T GetIndex (int Index)
	{
		switch (Index)
		{
			case 0:
				return buffer[index1];
				break;
			case 1:
				return buffer[index2];
				break;
			case 2:
				return buffer[index3];
				break;
			case 3:
				return buffer[index4];
				break;
            case 4:
				return buffer[index5];
				break;
			case 5:
				return buffer[index6];
				break;
			case 6:
				return buffer[index7];
				break;
			case 7:
				return buffer[index8];
				break;
			default:
				return buffer[index1];
				break;
		}
	}


	void SetLength (int Length)
    {
       if( Length >= maxLength )
			Length = maxLength;
	   if( Length < 0 )
			Length = 0;

        this->Length = Length;
    }


    void Clear()
    {
        memset(buffer, 0, sizeof(buffer));
		index1 = index2  = index3 = index4 = index5 = index6 = index7 = index8 = 0;
    }


    int GetLength() const
    {
        return Length;
    }
};

template<typename T, int OverSampleCount>
    class StateVariable
    {
    public:

        enum FilterType
        {
            LOWPASS,
            HIGHPASS,
            BANDPASS,
            NOTCH,
            FilterTypeCount
        };

    private:

        T sampleRate = {};
        T frequency = {};
        T q = {};
        T f = {};

        T low = {};
        T high = {};
        T band = {};
        T notch = {};

        T *out = nullptr;

        // folded form of the OverSampleCount-iteration loop (see UpdateCoefficient)
        T cA00 = {}, cA01 = {}, cA10 = {}, cA11 = {};
        T cB0 = {}, cB1 = {};
        T cC0 = {}, cC1 = {};

    public:
        StateVariable()
        {
            SetSampleRate(44100.);
            Frequency(1000.);
            Resonance(0);
            Type(LOWPASS);
            Reset();
        }

        T operator()(T input)
        {
            for(unsigned int i = 0; i < OverSampleCount; i++)
            {
                low += static_cast<T>(f * band + 1e-25);
                high = input - low - q * band;
                band += f * high;
                notch = low + high;
            }
			return *out;
        }

        // Folded equivalent of operator(): the oversampling loop is affine in
        // (low, band), so its OverSampleCount iterations collapse into a single
        // precomputed matrix step, exact in real arithmetic, and a ~4x shorter
        // serial dependency chain. LOWPASS output only (high/notch stay stale),
        // which is all MVerb uses.
        T Tick (T input)
        {
            const T l = cA00 * low + cA01 * band + cB0 * input + cC0;
            const T b = cA10 * low + cA11 * band + cB1 * input + cC1;
            low = l;
            band = b;
            return low;
        }

        void Reset()
        {
            low = high = band = notch = 0;
        }

        void SetSampleRate(T sampleRate)
        {
            this->sampleRate = sampleRate * OverSampleCount;
            UpdateCoefficient();
        }

        void Frequency(T frequency)
        {
            if ( frequency == this->frequency )
                return;		// unchanged, skip the sinf + matrix fold
            this->frequency = frequency;
            UpdateCoefficient();
        }

        void Resonance(T resonance)
        {
            this->q = 2 - 2 * resonance;
            UpdateCoefficient();
        }

        void Type(int type)
        {
            switch(type)
            {
            case LOWPASS:
                out = &low;
                break;

            case HIGHPASS:
                out = &high;
                break;

            case BANDPASS:
                out = &band;
                break;

            case NOTCH:
                out = &notch;
                break;

            default:
                out = &low;
                break;
            }
        }

    private:
        void UpdateCoefficient()
        {
            f = static_cast<T>(2. * sinf(3.141592654 * frequency / sampleRate));

            // Fold the OverSampleCount iterations of
            //   low += f*band + 1e-25;  high = in - low - q*band;  band += f*high
            // into one affine step  s' = A*s + B*in + C  with s = [low, band]:
            // per iteration s' = M*s + [0,f]*in + [eps,-f*eps], so over N
            // iterations A = M^N and B/C get the geometric sum S = sum M^k.
            const double fd = f;
            const double qd = q;
            const double m00 = 1.0,  m01 = fd;
            const double m10 = -fd,  m11 = 1.0 - fd * ( fd + qd );

            double a00 = 1.0, a01 = 0.0, a10 = 0.0, a11 = 1.0;	// M^k
            double s00 = 0.0, s01 = 0.0, s10 = 0.0, s11 = 0.0;	// sum of M^k, k < N
            for ( int k = 0; k < OverSampleCount; ++k )
            {
                s00 += a00; s01 += a01; s10 += a10; s11 += a11;
                const double t00 = m00 * a00 + m01 * a10;
                const double t01 = m00 * a01 + m01 * a11;
                const double t10 = m10 * a00 + m11 * a10;
                const double t11 = m10 * a01 + m11 * a11;
                a00 = t00; a01 = t01; a10 = t10; a11 = t11;
            }

            constexpr double eps = 1e-25;	// denormal guard from the iterated form
            cA00 = static_cast<T>( a00 );
            cA01 = static_cast<T>( a01 );
            cA10 = static_cast<T>( a10 );
            cA11 = static_cast<T>( a11 );
            cB0 = static_cast<T>( s01 * fd );
            cB1 = static_cast<T>( s11 * fd );
            cC0 = static_cast<T>( ( s00 - s01 * fd ) * eps );
            cC1 = static_cast<T>( ( s10 - s11 * fd ) * eps );
        }
	};
#endif
