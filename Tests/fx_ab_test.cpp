// Golden-output regression + performance test for the ultraSID FX / EQ
// components. Each entry processes a fixed deterministic 8 s stereo program
// (decorrelated noise, impulse, silent tail sections, sine) through one FX
// configured with the app's default settings; a parameter change mid-run
// exercises the smoothing paths:
//  - first run per entry: writes Tests/reference/fx/<name>.wav, records a
//    perf baseline, and verifies the render repeats bit-exactly in-process
//  - later runs: compares against the reference (all FX are deterministic,
//    fixed seeds and phases, so unchanged code reproduces it bit-exactly)
//    and times the FX against its baseline
//
// FX_Splitter is covered twice: two golden entries (the fixed 48 dB/oct
// crossover, and a mid-run frequency switch), plus an analytic self-check
// that verifies allpass-flat band summing and the low band's rolloff against
// the bilinear-warped Butterworth expectation.
//
// Exit codes, re-baselining and the machine-factor perf logic follow
// sidplay_ab_test.cpp: audio judged at -80 dB relative peak (exit 1), perf
// flagged when an entry stays >10 % above the median delta after a
// confirming re-measure, or the median exceeds +25 % (exit 2).
//
//   cmake --build --preset vs --config Release --target fx_ab_test
//   Builds/vs/Release/fx_ab_test.exe        (or ./Tests/test_fx.sh from the root)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Audio/Effects/FX_CheapTVSpeaker.h"
#include "Audio/Effects/FX_Delay.h"
#include "Audio/Effects/FX_Gain.h"
#include "Audio/Effects/FX_Noise.h"
#include "Audio/Effects/FX_Reverb.h"
#include "Audio/Effects/FX_Splitter.h"
#include "Audio/Effects/FX_TransformerHum.h"
#include "Audio/Effects/FX_WideMono.h"
#include "Audio/Effects/MultiBandEQ.h"

#include "test_common.h"

namespace fs = std::filesystem;
using namespace testcommon;

//-----------------------------------------------------------------------------

static constexpr int	SR = 44100;
static constexpr int	kProgSeconds = 8;
static constexpr uint32_t	kTotal = SR * kProgSeconds;
static constexpr int64_t	kSwitchPos = 4 * SR;	// mid-run parameter change

static unsigned rngState = 0x2468ACE1u;
static float frand ()
{
	rngState = rngState * 1664525u + 1013904223u;
	return static_cast<float>( ( rngState >> 8 ) & 0xFFFFFF ) / 8388608.0f - 1.0f;
}

// deterministic test program: noise, impulse, silence (FX tails), sine, silence
static void buildProgram ( std::vector<float>& L, std::vector<float>& R )
{
	L.assign ( kTotal, 0.0f );
	R.assign ( kTotal, 0.0f );

	for ( uint32_t i = 0; i < 3u * SR; ++i )
	{
		L[ i ] = frand () * 0.25f;
		R[ i ] = frand () * 0.25f;
	}
	L[ 3u * SR + 8820 ] = R[ 3u * SR + 8820 ] = 1.0f;	// impulse in the silence
	for ( uint32_t i = 5u * SR; i < 7u * SR; ++i )
	{
		const auto	ph = 2.0 * 3.14159265358979 * 440.0 * ( i - 5u * SR ) / SR;
		L[ i ] = static_cast<float>( 0.3 * sin ( ph ) );
		R[ i ] = static_cast<float>( 0.3 * sin ( ph + 0.5 ) );
	}
}

//-----------------------------------------------------------------------------
// one entry per FX: a factory building a fresh, app-configured instance and
// returning its per-chunk process closure (pos = absolute sample position,
// used for the mid-run parameter switch)

using ProcessFn = std::function<void ( float* const*, int, int64_t )>;

struct FxEntry
{
	const char*	name;
	std::function<ProcessFn ()>	make;
};

static const FxEntry	kEntries[] =
{
	{ "eq_loudness", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<MultiBandEQ> ();		// default 3-band loudness curve
		fx->setGainImmediate ( 0, 6.0 );					// app: lowGain 4 + 6581 bonus 2
		return [ fx, switched = false ] ( float* const* b, int n, int64_t pos ) mutable
		{
			if ( !switched && pos >= kSwitchPos )
			{
				switched = true;
				fx->setGain ( 0, 3.0 );						// ramped, exercises the smoothing path
			}
			fx->process ( b[ 0 ], b[ 1 ], n );
		};
	} },

	{ "tv_speaker", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_CheapTVSpeaker> ();	// 10-band EQ + tanh distortion
		fx->setDistortion ( 1.75f );							// app: real_distortion 0.175 * 10
		return [ fx ] ( float* const* b, int n, int64_t ) { fx->process ( b, n ); };
	} },

	{ "reverb", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_Reverb> ();
		fx->setWet ( 0.35f );								// pinned test value, deliberately not tracking app defaults
		return [ fx, switched = false ] ( float* const* b, int n, int64_t pos ) mutable
		{
			if ( !switched && pos >= kSwitchPos )
			{
				switched = true;
				fx->setWet ( 0.5f );
			}
			fx->process ( b, n );
		};
	} },

	{ "delay", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_Delay> ();
		fx->setFeedback ( 0.31f );							// pinned test values, deliberately not tracking app defaults
		fx->setWet ( 0.17f );
		fx->clearBuffers ();
		return [ fx ] ( float* const* b, int n, int64_t ) { fx->process ( b, n ); };
	} },

	{ "wide_mono", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_WideMono> ();
		fx->setWidth ( 0.5f );								// pinned test value, deliberately not tracking app defaults
		fx->clearBuffers ();
		return [ fx ] ( float* const* b, int n, int64_t ) { fx->process ( b, n ); };	// reads ch0, writes both
	} },

	{ "transformer_hum", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_TransformerHum> ();
		fx->setFrequency ( 50.0f );							// PAL
		fx->setVolume ( 0.08f );							// pinned test value, deliberately not tracking app defaults
		return [ fx ] ( float* const* b, int n, int64_t ) { fx->process ( b, n, 2 ); };
	} },

	{ "noise", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_Noise> ();
		fx->setVolume ( 0.12f );							// app: real_noise defaults
		fx->setColor ( 0.5f );
		return [ fx ] ( float* const* b, int n, int64_t ) { fx->process ( b, n ); };
	} },

	{ "gain", [] () -> ProcessFn
	{
		auto	fxL = std::make_shared<FX_Gain> ();
		auto	fxR = std::make_shared<FX_Gain> ();
		fxL->setGain ( 1.0f ); fxL->clearBuffers ();
		fxR->setGain ( 1.0f ); fxR->clearBuffers ();
		return [ fxL, fxR, switched = false ] ( float* const* b, int n, int64_t pos ) mutable
		{
			if ( !switched && pos >= kSwitchPos )
			{
				switched = true;
				fxL->setGain ( 0.5f );						// exercises the ramping path
				fxR->setGain ( 0.5f );
			}
			fxL->process ( b[ 0 ], n );						// app applies gain per channel
			fxR->process ( b[ 1 ], n );
		};
	} },

	// Moves the crossover mid-render, so it also covers the deferred coefficient
	// update: setFrequency only flags, splitBands derives
	{ "splitter_freq_switch", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_Splitter> ();
		fx->setFrequency ( 800.0f );						// pinned, immune to class-default changes
		return [ fx, switched = false ] ( float* const* b, int n, int64_t pos ) mutable
		{
			if ( !switched && pos >= kSwitchPos )
			{
				switched = true;
				fx->setFrequency ( 350.0f );				// app: EPIC splitter frequency
			}
			fx->splitBands ( b, n, 2 );
			for ( auto ch = 0; ch < 2; ++ch )				// halve the high band, so the
				for ( auto i = 0; i < n; ++i )				// reference captures both bands
					b[ ch ][ i ] *= 0.5f;
			fx->mergeBands ( b, n );
		};
	} },

	{ "splitter_48db", [] () -> ProcessFn
	{
		auto	fx = std::make_shared<FX_Splitter> ();
		fx->setFrequency ( 800.0f );
		return [ fx ] ( float* const* b, int n, int64_t )
		{
			fx->splitBands ( b, n, 2 );
			for ( auto ch = 0; ch < 2; ++ch )
				for ( auto i = 0; i < n; ++i )
					b[ ch ][ i ] *= 0.5f;
			fx->mergeBands ( b, n );
		};
	} },
};

//-----------------------------------------------------------------------------
// analytic crossover self-check: a steady sine summed back
// from both bands must come out at unity gain (the LR band sum is allpass),
// and the low band alone must attenuate like the bilinear-warped squared
// Butterworth, so the expectations are exact and tolerances stay tight

static constexpr double	kSplitFc = 800.0;

// output level of a steady sine in dB relative to the input, through
// split + merge (both bands) or the low band alone
static double splitterSineDb ( FX_Splitter& fx, const double f, const bool lowBandOnly )
{
	static constexpr double		amp = 0.5;
	static constexpr uint32_t	total = SR;		// 1 s, the tail measurement leaves ~0.6 s to settle

	std::vector<float>	L ( total ), R ( total );
	for ( uint32_t i = 0; i < total; ++i )
		L[ i ] = R[ i ] = static_cast<float>( amp * sin ( 2.0 * 3.14159265358979 * f * i / SR ) );

	fx.reset ();
	for ( uint32_t off = 0; off < total; off += 512 )
	{
		const auto	n = static_cast<int>( std::min<uint32_t> ( 512, total - off ) );
		float*	b[ 2 ] = { L.data () + off, R.data () + off };
		fx.splitBands ( b, n, 2 );
		if ( lowBandOnly )
		{
			std::fill_n ( b[ 0 ], n, 0.0f );
			std::fill_n ( b[ 1 ], n, 0.0f );
		}
		fx.mergeBands ( b, n );
	}

	// RMS over an integer number of periods at the tail keeps leakage out
	const auto	periods = static_cast<uint32_t>( 0.4 * f );
	const auto	len = static_cast<uint32_t>( periods * SR / f + 0.5 );
	auto	sum = 0.0;
	for ( auto i = total - len; i < total; ++i )
		sum += double ( L[ i ] ) * L[ i ];

	return 20.0 * log10 ( std::sqrt ( sum / len ) * std::sqrt ( 2.0 ) / amp );
}

static bool splitterSelfCheck ()
{
	auto	allOk = true;

	{
		constexpr auto	slope = 48;		// LR8, the only order the splitter builds

		FX_Splitter	fx;
		fx.setFrequency ( static_cast<float>( kSplitFc ) );

		auto	maxFlat = 0.0;
		for ( const auto f : { 100.0, 300.0, 800.0, 2000.0, 6000.0, 15000.0 } )
			maxFlat = std::max ( maxFlat, std::fabs ( splitterSineDb ( fx, f, false ) ) );

		// low band at one and two octaves above the crossover vs the exact
		// digital-domain expectation |H| = 1 / (1 + xw^2N), xw bilinear-warped
		double	low[ 2 ], expect[ 2 ];
		for ( auto i = 0; i < 2; ++i )
		{
			const auto	f = kSplitFc * ( i == 0 ? 2.0 : 4.0 );
			const auto	xw = tan ( 3.14159265358979 * f / SR ) / tan ( 3.14159265358979 * kSplitFc / SR );
			low[ i ] = splitterSineDb ( fx, f, true );
			expect[ i ] = -20.0 * log10 ( 1.0 + pow ( xw, slope / 6.0 ) );
		}

		const auto	ok = maxFlat < 0.05
						&& std::fabs ( low[ 0 ] - expect[ 0 ] ) < 0.2
						&& std::fabs ( low[ 1 ] - expect[ 1 ] ) < 0.2;
		if ( !ok )
			allOk = false;

		char	name[ 32 ];
		snprintf ( name, sizeof ( name ), "splitter check %ddb", slope );
		printf ( "%-20s %s  flat %.3f dB, low %.1f/%.1f dB @2fc/4fc (expect %.1f/%.1f)\n",
				 name, ok ? "ok  " : "FAIL", maxFlat, low[ 0 ], low[ 1 ], expect[ 0 ], expect[ 1 ] );
	}

	return allOk;
}

//-----------------------------------------------------------------------------

// process the whole program through a fresh FX instance, varied chunk sizes
static void renderFx ( const FxEntry& e, const std::vector<float>& progL, const std::vector<float>& progR,
					   std::vector<float>& outL, std::vector<float>& outR )
{
	outL = progL;
	outR = progR;
	auto	proc = e.make ();

	static constexpr int	sizes[] = { 735, 441, 100, 256, 64, 733, 17, 512 };
	int64_t	pos = 0;
	auto	si = 0;
	while ( pos < static_cast<int64_t>( kTotal ) )
	{
		const auto	n = static_cast<int>( std::min<int64_t> ( sizes[ si++ % 8 ], kTotal - pos ) );
		float*	b[ 2 ] = { outL.data () + pos, outR.data () + pos };
		proc ( b, n, pos );
		pos += n;
	}
}

// seconds of CPU per second of audio, repeated until the measurement is long
// enough to be meaningful (FX are hundreds of times faster than realtime)
static double measureFx ( const FxEntry& e, const std::vector<float>& progL, const std::vector<float>& progR )
{
	auto	proc = e.make ();
	auto	L = progL;
	auto	R = progR;

	auto	reps = 0;
	int64_t	pos = 0;
	const auto	t0 = std::chrono::steady_clock::now ();
	double	elapsed;
	do
	{
		for ( uint32_t off = 0; off < kTotal; off += 735 )
		{
			const auto	n = static_cast<int>( std::min<uint32_t> ( 735, kTotal - off ) );
			float*	b[ 2 ] = { L.data () + off, R.data () + off };
			proc ( b, n, pos );
			pos += n;
		}
		++reps;
		elapsed = std::chrono::duration<double> ( std::chrono::steady_clock::now () - t0 ).count ();
	} while ( elapsed < 0.25 );

	return elapsed / ( double ( kProgSeconds ) * reps );
}

//-----------------------------------------------------------------------------

int main ()
{
	const fs::path	root = FX_TEST_ROOT;
	const fs::path	refDir = root / "Tests" / "reference" / "fx";
	const fs::path	baselinePath = refDir / "perf-baseline.txt";

	std::error_code	ec;
	fs::create_directories ( refDir, ec );

	std::vector<float>	progL, progR;
	buildProgram ( progL, progR );

	auto	baseline = readBaseline ( baselinePath );
	auto	baselineDirty = false;
	auto	audioFail = false;
	auto	perfFail = false;

	struct PerfInfo
	{
		const FxEntry*	entry;
		std::string	name;
		double	delta;
	};
	std::vector<PerfInfo>	perfInfos;

	if ( !splitterSelfCheck () )
		audioFail = true;

	for ( const auto& e : kEntries )
	{
		const auto	name = std::string ( e.name );
		printf ( "%-20s ", name.c_str () );

		std::vector<float>	outL, outR;
		renderFx ( e, progL, progR, outL, outR );

		const auto	spa = measureFx ( e, progL, progR );	// seconds per audio-second
		const auto	speed = 1.0 / spa;
		const auto	refPath = refDir / ( name + ".wav" );

		if ( !fs::exists ( refPath ) )
		{
			if ( !writeWavF32 ( refPath, outL.data (), outR.data (), kTotal, SR ) )
			{
				printf ( "FAIL (could not write reference)\n" );
				audioFail = true;
				continue;
			}
			baseline[ name ] = { kProgSeconds, spa };
			baselineDirty = true;

			// determinism guard: a fresh instance must reproduce bit-exactly
			std::vector<float>	rL, rR;
			renderFx ( e, progL, progR, rL, rR );
			const auto	repeatExact = rL == outL && rR == outR;
			if ( !repeatExact )
				audioFail = true;
			printf ( "reference created  (%.0fx realtime, %s)\n", speed, repeatExact ? "repeat bit-exact" : "REPEAT DIFFERS - NOT DETERMINISTIC" );
			continue;
		}

		// --- compare against the reference
		Wav	ref;
		if ( !readWavF32 ( refPath, ref ) )
		{
			printf ( "FAIL (unreadable reference %s)\n", refPath.string ().c_str () );
			audioFail = true;
			continue;
		}
		if ( ref.channels != 2 || ref.frames () != kTotal )
		{
			printf ( "FAIL (layout changed - delete the reference to re-baseline)\n" );
			audioFail = true;
			continue;
		}

		auto	peakDiff = 0.0, peakSig = 0.0;
		auto	firstDiff = -1.0, maxDiffAt = -1.0;
		for ( uint32_t i = 0; i < kTotal; ++i )
		{
			for ( auto c = 0; c < 2; ++c )
			{
				const double	r = ref.samples[ static_cast<size_t>( i ) * 2 + c ];
				const double	v = c == 0 ? outL[ i ] : outR[ i ];
				const auto	d = std::fabs ( v - r );
				if ( d > 0.0 && firstDiff < 0.0 )
					firstDiff = i / double ( SR );
				if ( d > peakDiff )
				{
					peakDiff = d;
					maxDiffAt = i / double ( SR );
				}
				peakSig = std::max ( peakSig, std::fabs ( r ) );
			}
		}

		const auto	relDb = ( peakDiff > 0.0 && peakSig > 0.0 ) ? 20.0 * std::log10 ( peakDiff / peakSig ) : -999.0;
		const auto	audioOK = relDb < -80.0;	// audibility bar
		if ( !audioOK )
			audioFail = true;

		char	audioTxt[ 96 ];
		if ( peakDiff == 0.0 )
			snprintf ( audioTxt, sizeof ( audioTxt ), "bit-exact" );
		else
			snprintf ( audioTxt, sizeof ( audioTxt ), "diff %.1f dB (first @%.3fs, max @%.3fs)", relDb, firstDiff, maxDiffAt );

		// --- performance vs baseline (verdict deferred to machine-factor stage)
		char	perfTxt[ 96 ];
		const auto	it = baseline.find ( name );
		if ( it != baseline.end () && it->second.first == kProgSeconds )
		{
			const auto	delta = ( spa / it->second.second - 1.0 ) * 100.0;
			perfInfos.push_back ( { &e, name, delta } );
			snprintf ( perfTxt, sizeof ( perfTxt ), "%.0fx realtime, %+.1f%% vs baseline", speed, delta );
		}
		else
		{
			baseline[ name ] = { kProgSeconds, spa };
			baselineDirty = true;
			snprintf ( perfTxt, sizeof ( perfTxt ), "%.0fx realtime, baseline recorded", speed );
		}

		printf ( "%s  %-14s  %s\n", audioOK ? "ok  " : "FAIL", audioTxt, perfTxt );
	}

	// --- performance verdict (see sidplay_ab_test.cpp for the rationale)
	if ( !perfInfos.empty () )
	{
		auto	deltas = perfInfos;
		std::sort ( deltas.begin (), deltas.end (), [] ( const PerfInfo& a, const PerfInfo& b ) { return a.delta < b.delta; } );
		const auto	median = deltas[ deltas.size () / 2 ].delta;
		printf ( "\nmachine factor (median vs baselines): %+.1f%%\n", median );

		for ( const auto& p : perfInfos )
		{
			if ( p.delta - median <= 10.0 )
				continue;

			const auto	it = baseline.find ( p.name );
			auto	confirmed = p.delta;
			if ( it != baseline.end () )
				confirmed = std::min ( confirmed, ( measureFx ( *p.entry, progL, progR ) / it->second.second - 1.0 ) * 100.0 );
			if ( confirmed - median > 10.0 )
			{
				perfFail = true;
				printf ( "PERF REGRESSION: %s %+.1f%% vs baseline (%+.1f%% above machine factor, re-measure confirmed)\n",
						 p.name.c_str (), confirmed, confirmed - median );
			}
		}

		if ( median > 25.0 )
		{
			perfFail = true;
			printf ( "PERF REGRESSION: uniform slowdown of %+.1f%% across all FX - beyond machine-state range\n", median );
		}
	}

	if ( baselineDirty )
		writeBaseline ( baselinePath, baseline );

	printf ( "\n%s\n", audioFail ? "FAIL (audio)" : perfFail ? "PASS audio, FAIL performance" : "PASS" );
	return audioFail ? 1 : perfFail ? 2 : 0;
}
