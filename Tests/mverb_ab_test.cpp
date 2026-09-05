// A/B equivalence test + benchmark for MVerb's SIMD block path.
//
// Verifies that MVerb's block-based process path (SIMD kernels, folded SVF,
// block tap reads) produces the same output as the original per-sample loop
// (processScalar), across kernel variants (SSE vs AVX2), mixed buffer sizes,
// mid-run SIZE/DECAY changes, and the isolation corners EARLYMIX=0/1.
// FAIL limit is -80 dB relative peak (the audibility threshold; differences
// below that are fine and preferred over lost performance); the current
// implementation agrees at float-ulp level (~ -140 dB) once smoothed
// parameters have settled, so the printed numbers also flag
// inaudible-but-real regressions early. The first 2 s are excluded from the
// stats (one-time startup-ramp transient, see the `skip` comment in runTest).
// Also times scalar vs block over 60 s of audio.
//
// On-demand target, not part of the normal build:
//   cmake --build --preset vs --config Release --target mverb_ab_test
//   Builds/vs/Release/mverb_ab_test.exe

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#if defined(_MSC_VER)
	#include <intrin.h>
#endif

#if defined(__x86_64__) || defined(_M_X64)
	#include <immintrin.h>
#endif

// test-only: expose processScalar/processBlock/chunkLimit/useAVX2. This TU
// links against nothing else that includes MVerb.h, so there is no ODR pairing
// to violate.
#define private public
#include "Audio/Effects/MVerb.h"
#undef private

static unsigned rngState = 0x12345678u;
static float frand ()
{
	rngState = rngState * 1664525u + 1013904223u;
	return static_cast<float>( ( rngState >> 8 ) & 0xFFFFFF ) / 8388608.0f - 1.0f;
}

struct Stats
{
	double peakDiff = 0.0, sumSq = 0.0, peakSig = 0.0;
	long long n = 0;

	void add ( const float a, const float b )
	{
		const double d = std::fabs ( static_cast<double>( a ) - static_cast<double>( b ) );
		peakDiff = std::max ( peakDiff, d );
		sumSq += d * d;
		++n;
		peakSig = std::max ( peakSig, static_cast<double>( std::fabs ( a ) ) );
	}
	double rms () const { return n ? std::sqrt ( sumSq / n ) : 0.0; }
	double relPeak () const { return peakSig > 0.0 ? peakDiff / peakSig : 0.0; }
};

// returns the worst scalar-vs-block relative peak difference of the run
static double runTest ( const float density, const bool midRunSizeChange, const float earlyMix )
{
	printf ( "\n=== DENSITY=%.2f, mid-run SIZE change: %s, EARLYMIX=%.2f ===\n",
			 density, midRunSizeChange ? "yes" : "no", earlyMix );

	const int SR = 44100;
	const int total = SR * 6;
	// Parameter-settling region, excluded from stats: the block path updates
	// filter coefficients at chunk rate, so the startup ramps apply at slightly
	// different times than the scalar reference, and the reverb tail carries
	// the memory of that one-time difference for ~2 s (decaying, -70 dB after
	// 1 s). Persistent divergences (real bugs) don't decay and are still caught.
	const int skip = SR * 2;

	// program: 1s stereo noise, impulse, silence, 1s decorrelated noise, long tail
	std::vector<float> inL ( total ), inR ( total );
	for ( int i = 0; i < total; ++i )
	{
		float v = 0.0f;
		if ( i < SR )						v = frand () * 0.5f;
		else if ( i == SR + 100 )			v = 1.0f;
		else if ( i >= 2 * SR && i < 3 * SR )	v = frand () * 0.25f;
		inL[ i ] = v;
		inR[ i ] = ( i >= 2 * SR && i < 3 * SR ) ? frand () * 0.25f : v;
	}

	// ~400 KB of delay lines each -> heap, not stack
	auto pA = std::make_unique<MVerb<float>> (), pB = std::make_unique<MVerb<float>> (), pC = std::make_unique<MVerb<float>> ();
	auto& A = *pA;	// scalar reference
	auto& B = *pB;	// block path, default kernels
	auto& C = *pC;	// block path, SSE kernels forced
	const auto setup = [ density, earlyMix ] ( MVerb<float>& m )
	{
		m.setParameter ( MVerb<float>::DAMPINGFREQ, 0.7f );
		m.setParameter ( MVerb<float>::DENSITY, density );
		m.setParameter ( MVerb<float>::BANDWIDTHFREQ, 0.9f );
		m.setParameter ( MVerb<float>::DECAY, 0.5f );
		m.setParameter ( MVerb<float>::PREDELAY, 0.1f );
		m.setParameter ( MVerb<float>::SIZE, 1.0f );
		m.setParameter ( MVerb<float>::GAIN, 1.0f );
		m.setParameter ( MVerb<float>::MIX, 0.125f );
		m.setParameter ( MVerb<float>::EARLYMIX, earlyMix );
	};
	setup ( A ); setup ( B ); setup ( C );
	C.useAVX2 = false;

	printf ( "chunkLimit (SIZE=1.0): %d   useAVX2 default: %d\n", B.chunkLimit, static_cast<int>( B.useAVX2 ) );

	std::vector<float> aL ( total ), aR ( total ), bL ( total ), bR ( total ), cL ( total ), cR ( total );

	const int sizes[] = { 64, 441, 100, 256, 1, 17, 512, 333 };
	int pos = 0, si = 0;
	bool switched = false;
	bool tankReported = false;
	while ( pos < total )
	{
		if ( midRunSizeChange && !switched && pos >= 3 * SR )
		{
			// mid-run parameter change: smallest room (worst-case tap ages) + long decay
			switched = true;
			for ( auto* m : { &A, &B, &C } )
			{
				m->setParameter ( MVerb<float>::SIZE, 0.0f );
				m->setParameter ( MVerb<float>::DECAY, 0.9f );
			}
			printf ( "chunkLimit (SIZE=0.0): %d\n", B.chunkLimit );
		}

		const int n = std::min ( sizes[ si++ % 8 ], total - pos );
		float* ia[ 2 ] = { inL.data () + pos, inR.data () + pos };
		float* oa[ 2 ] = { aL.data () + pos, aR.data () + pos };
		float* ob[ 2 ] = { bL.data () + pos, bR.data () + pos };
		float* oc[ 2 ] = { cL.data () + pos, cR.data () + pos };

		A.processScalar ( ia, oa, n );
		B.process ( ia, ob, n );
		C.process ( ia, oc, n );

		// track divergence of the tank state itself (feedback path), separate
		// from the output taps: distinguishes a serial-loop bug from an
		// accumulator-read bug
		const double tankDiff = std::max (
			std::fabs ( static_cast<double>( A.PreviousLeftTank ) - B.PreviousLeftTank ),
			std::fabs ( static_cast<double>( A.PreviousRightTank ) - B.PreviousRightTank ) );
		if ( pos >= skip && !tankReported && tankDiff > 1.0e-4 )
		{
			tankReported = true;
			printf ( "tank state diverged > 1e-4 after sample %d (diff %.3e)\n", pos + n, tankDiff );
		}

		pos += n;
	}

	for ( int i = skip; i < total; ++i )
	{
		if ( std::fabs ( static_cast<double>( aL[ i ] ) - bL[ i ] ) > 1.0e-4 )
		{
			printf ( "first output diff > 1e-4 at sample %d: scalar=%.8f block=%.8f\n", i, aL[ i ], bL[ i ] );
			break;
		}
	}

	// diffs measured after the settle region, but judged relative to the peak
	// of the WHOLE program: audibility is about the material the listener
	// hears, and the loud sections sit before the skip point
	Stats sAB, sAC, sBC;
	for ( int i = skip; i < total; ++i )
	{
		sAB.add ( aL[ i ], bL[ i ] ); sAB.add ( aR[ i ], bR[ i ] );
		sAC.add ( aL[ i ], cL[ i ] ); sAC.add ( aR[ i ], cR[ i ] );
		sBC.add ( bL[ i ], cL[ i ] ); sBC.add ( bR[ i ], cR[ i ] );
	}
	double globalPeak = 0.0;
	for ( int i = 0; i < total; ++i )
	{
		globalPeak = std::max ( globalPeak, static_cast<double>( std::fabs ( aL[ i ] ) ) );
		globalPeak = std::max ( globalPeak, static_cast<double>( std::fabs ( aR[ i ] ) ) );
	}
	sAB.peakSig = sAC.peakSig = sBC.peakSig = globalPeak;

	// per-second peak diff (scalar vs block): a real tap bug shows up loud
	// immediately, rounding-sensitivity starts near float epsilon and grows
	printf ( "scalar vs block, per-second peak diff: " );
	for ( int sec = 0; sec < total / SR; ++sec )
	{
		double pk = 0.0;
		for ( int i = sec * SR; i < ( sec + 1 ) * SR; ++i )
		{
			pk = std::max ( pk, std::fabs ( static_cast<double>( aL[ i ] ) - bL[ i ] ) );
			pk = std::max ( pk, std::fabs ( static_cast<double>( aR[ i ] ) - bR[ i ] ) );
		}
		printf ( "%.1e  ", pk );
	}
	printf ( "\n" );

	const auto report = [] ( const char* name, const Stats& s )
	{
		const double rel = ( s.peakDiff > 0.0 && s.peakSig > 0.0 ) ? 20.0 * std::log10 ( s.relPeak () ) : -999.0;
		printf ( "%s: peakSig=%.6f  peakDiff=%.3e  rmsDiff=%.3e  rel peak=%.1f dB\n",
				 name, s.peakSig, s.peakDiff, s.rms (), rel );
	};
	report ( "scalar vs block(default)", sAB );
	report ( "scalar vs block(SSE)    ", sAC );
	report ( "block default vs SSE    ", sBC );

	return std::max ( sAB.relPeak (), sAC.relPeak () );
}

static void benchmark ()
{
	const int SR = 44100;
	const int total = SR * 60;
	const int block = 441;

	std::vector<float> inL ( total ), inR ( total ), outL ( total ), outR ( total );
	for ( int i = 0; i < total; ++i )
	{
		inL[ i ] = frand () * 0.5f;
		inR[ i ] = frand () * 0.5f;
	}

	const auto run = [ & ] ( const bool scalar, const bool avx2 ) -> double
	{
		auto pM = std::make_unique<MVerb<float>> ();
		auto& M = *pM;
		M.setParameter ( MVerb<float>::DENSITY, 1.0f );
		M.setParameter ( MVerb<float>::MIX, 0.125f );
		M.setParameter ( MVerb<float>::EARLYMIX, 0.75f );
		M.useAVX2 = avx2;

		const auto t0 = std::chrono::steady_clock::now ();
		for ( int pos = 0; pos < total; pos += block )
		{
			float* ia[ 2 ] = { inL.data () + pos, inR.data () + pos };
			float* oa[ 2 ] = { outL.data () + pos, outR.data () + pos };
			if ( scalar )
				M.processScalar ( ia, oa, block );
			else
				M.process ( ia, oa, block );
		}
		const auto t1 = std::chrono::steady_clock::now ();
		return std::chrono::duration<double> ( t1 - t0 ).count ();
	};

	run ( true, true );		// warm-up
	const auto tScalar = run ( true, true );
	const auto tSSE = run ( false, false );
	const auto tAVX2 = run ( false, true );
	printf ( "\n=== 60 s stereo @ 44.1 kHz, block %d ===\n", block );
	printf ( "scalar      : %7.2f ms  (%5.1fx realtime)\n", tScalar * 1000.0, 60.0 / tScalar );
	printf ( "block SSE   : %7.2f ms  (%5.1fx realtime)  %.2fx faster\n", tSSE * 1000.0, 60.0 / tSSE, tScalar / tSSE );
	printf ( "block AVX2  : %7.2f ms  (%5.1fx realtime)  %.2fx faster\n", tAVX2 * 1000.0, 60.0 / tAVX2, tScalar / tAVX2 );
}

int main ()
{
#if MVERB_SSE
	printf ( "runtime AVX2+FMA: %s\n", mverb_kernels::hasAVX2 () ? "yes" : "no" );
#elif MVERB_NEON
	printf ( "NEON build\n" );
#endif

	auto worst = 0.0;
	worst = std::max ( worst, runTest ( 1.0f, false, 0.0f ) );	// wet = early reflections only -> isolates the ER block phase
	worst = std::max ( worst, runTest ( 1.0f, false, 1.0f ) );	// wet = tank/accumulator only -> isolates tank + output taps + SVFs
	worst = std::max ( worst, runTest ( 1.0f, true, 0.75f ) );	// the app's config
	worst = std::max ( worst, runTest ( 0.9f, true, 0.75f ) );	// strictly stable feedback

	const bool pass = worst < 1.0e-4;	// -80 dB = audibility threshold (current level is ~ -140 dB, float ulp)
	printf ( "\n%s  (worst rel peak %.1f dB, limit -80 dB)\n", pass ? "PASS" : "FAIL", 20.0 * std::log10 ( std::max ( worst, 1e-30 ) ) );

	benchmark ();

	return pass ? 0 : 1;
}
