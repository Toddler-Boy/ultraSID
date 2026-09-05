// PerceivedLoudness validation suite:
//   EBU Tech 3341 minimum set (cases 1-5; case 6 is 5.0-channel, out of scope),
//   dual mono equivalence, chunk invariance, progressive readout, and silence
//   handling. Prints PASS/FAIL, exits non-zero on FAIL.
//
//   cmake --build --preset vs --config Release --target perceived_loudness_test
//   Builds/vs/Release/perceived_loudness_test.exe

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector>

#include "Audio/PerceivedLoudness.h"

static constexpr auto	SR = 44100.0;
static constexpr auto	pi = 3.141592653589793;

static int	failures = 0;

//-----------------------------------------------------------------------------

static void check ( const bool ok, const char* fmt, ... )
{
	char	line[ 512 ];
	va_list	args;
	va_start ( args, fmt );
	vsnprintf ( line, sizeof ( line ), fmt, args );
	va_end ( args );

	printf ( "%s  %s\n", ok ? "PASS" : "FAIL", line );
	if ( ! ok )
		++failures;
}
//-----------------------------------------------------------------------------

struct Segment
{
	double	dbfs;
	double	seconds;
};

// 997 Hz per Tech 3341, phase-continuous across segments
static std::vector<float> sineSequence ( const std::vector<Segment>& segments )
{
	std::vector<float>	out;
	auto	phase = 0.0;

	for ( const auto& seg : segments )
	{
		const auto	amp = std::pow ( 10.0, seg.dbfs / 20.0 );
		const auto	frames = int ( seg.seconds * SR + 0.5 );

		for ( auto i = 0; i < frames; ++i )
		{
			out.push_back ( float ( amp * std::sin ( phase ) ) );
			phase += 2.0 * pi * 997.0 / SR;
			if ( phase > 2.0 * pi )
				phase -= 2.0 * pi;
		}
	}
	return out;
}
//-----------------------------------------------------------------------------

// Deterministic broadband material: two detuned sines + LCG noise under a
// slow amplitude envelope, distinct per seed
static std::vector<float> noisySignal ( const double seconds, uint32_t seed )
{
	std::vector<float>	out;
	const auto	frames = int ( seconds * SR );
	auto	phase1 = 0.0, phase2 = 0.0;

	for ( auto i = 0; i < frames; ++i )
	{
		seed = seed * 1664525u + 1013904223u;
		const auto	noise = ( double ( seed >> 8 ) / double ( 1u << 24 ) - 0.5 ) * 0.2;
		const auto	env = 0.35 + 0.25 * std::sin ( 2.0 * pi * 0.3 * i / SR );

		out.push_back ( float ( env * ( 0.5 * std::sin ( phase1 ) + 0.3 * std::sin ( phase2 ) + noise ) ) );
		phase1 += 2.0 * pi * 440.0 / SR;
		phase2 += 2.0 * pi * 3123.0 / SR;
	}
	return out;
}
//-----------------------------------------------------------------------------

static void feed ( PerceivedLoudness& ebu, const float* L, const float* R, const size_t frames, const size_t chunk )
{
	for ( size_t off = 0; off < frames; off += chunk )
	{
		const auto	n = int ( std::min ( chunk, frames - off ) );
		const float*	ch[ 2 ] = { L + off, R ? R + off : nullptr };
		ebu.process ( ch, n );
	}
}
//-----------------------------------------------------------------------------

static double measureStereo ( const std::vector<float>& L, const std::vector<float>& R )
{
	PerceivedLoudness	ebu ( SR, 2 );
	feed ( ebu, L.data (), R.data (), L.size (), L.size () );
	return ebu.integratedLUFS ();
}
//-----------------------------------------------------------------------------

static void tech3341 ()
{
	const struct
	{
		const char*	name;
		std::vector<Segment>	segments;
		double	expected;
	}
	cases[] =
	{
		{ "3341 case 1 (-23 dBFS tone)",	{ { -23.0, 20.0 } },										-23.0 },
		{ "3341 case 2 (-33 dBFS tone)",	{ { -33.0, 20.0 } },										-33.0 },
		{ "3341 case 3 (rel gate)",			{ { -36.0, 10.0 }, { -23.0, 60.0 }, { -36.0, 10.0 } },		-23.0 },
		{ "3341 case 4 (abs+rel gate)",		{ { -72.0, 10.0 }, { -36.0, 10.0 }, { -23.0, 60.0 },
											  { -36.0, 10.0 }, { -72.0, 10.0 } },						-23.0 },
		{ "3341 case 5 (loud middle)",		{ { -26.0, 20.0 }, { -20.0, 20.1 }, { -26.0, 20.0 } },		-23.0 },
	};

	for ( const auto& c : cases )
	{
		const auto	tone = sineSequence ( c.segments );
		const auto	lufs = measureStereo ( tone, tone );
		check ( std::fabs ( lufs - c.expected ) <= 0.1, "%s: %.3f LUFS (expect %.1f +/-0.1)", c.name, lufs, c.expected );
	}
}
//-----------------------------------------------------------------------------

static void dualMono ()
{
	const auto	tone = sineSequence ( { { -23.0, 20.0 } } );

	PerceivedLoudness	mono ( SR, 1 );
	feed ( mono, tone.data (), nullptr, tone.size (), tone.size () );

	const auto	monoLufs = mono.integratedLUFS ();
	const auto	stereoLufs = measureStereo ( tone, tone );

	check ( monoLufs == stereoLufs, "dual mono: mono %.6f == duplicated stereo %.6f LUFS", monoLufs, stereoLufs );
	check ( std::fabs ( monoLufs + 23.0 ) <= 0.1, "dual mono absolute: %.3f LUFS (expect -23.0 +/-0.1)", monoLufs );
}
//-----------------------------------------------------------------------------

static void chunkInvariance ()
{
	const auto	L = noisySignal ( 30.0, 1u );
	const auto	R = noisySignal ( 30.0, 2u );

	PerceivedLoudness	ref ( SR, 2 );
	feed ( ref, L.data (), R.data (), L.size (), L.size () );

	for ( const size_t chunk : { size_t ( 1 ), size_t ( 63 ), size_t ( 512 ), size_t ( 4096 ) } )
	{
		PerceivedLoudness	ebu ( SR, 2 );
		feed ( ebu, L.data (), R.data (), L.size (), chunk );

		const auto	same = ebu.integratedLUFS () == ref.integratedLUFS ()
						&& ebu.effectiveLUFS () == ref.effectiveLUFS ()
						&& ebu.midLUFS () == ref.midLUFS ()
						&& ebu.samplePeak () == ref.samplePeak ();
		check ( same, "chunk invariance: chunk %zu bit-identical to whole-file", chunk );
	}
}
//-----------------------------------------------------------------------------

static void progressive ()
{
	const auto	L = noisySignal ( 30.0, 3u );
	const auto	R = noisySignal ( 30.0, 4u );

	PerceivedLoudness	ebu ( SR, 2 );

	const auto	slice = size_t ( SR * 0.1 );
	auto	sane = true;
	auto	deviationAt20s = 0.0;

	for ( size_t off = 0; off < L.size (); off += slice )
	{
		const auto	n = int ( std::min ( slice, L.size () - off ) );
		const float*	ch[ 2 ] = { L.data () + off, R.data () + off };
		ebu.process ( ch, n );

		const auto	q = ebu.integratedLUFS ();
		if ( off + slice >= size_t ( SR * 0.4 ) )
			sane = sane && std::isfinite ( q ) && std::isfinite ( ebu.effectiveLUFS () );
		if ( off + slice == size_t ( SR * 20.0 ) )
			deviationAt20s = q;
	}

	const auto	final_ = ebu.integratedLUFS ();
	check ( sane, "progressive: finite readout from 400 ms on" );
	check ( std::fabs ( deviationAt20s - final_ ) < 1.0, "progressive: 20 s readout %.3f within 1 LU of final %.3f", deviationAt20s, final_ );

	// A const query must not disturb the measurement
	PerceivedLoudness	silent ( SR, 2 );
	feed ( silent, L.data (), R.data (), L.size (), slice );
	check ( silent.integratedLUFS () == final_, "progressive: mid-render queries leave the result untouched" );
}
//-----------------------------------------------------------------------------

static void silence ()
{
	const std::vector<float>	zeros ( size_t ( SR * 5 ), 0.0f );

	PerceivedLoudness	ebu ( SR, 2 );
	feed ( ebu, zeros.data (), zeros.data (), zeros.size (), 512 );

	check ( ebu.integratedLUFS () == -HUGE_VAL && ebu.effectiveLUFS () == -HUGE_VAL,
			"silence: integrated and effective read -inf" );

	PerceivedLoudness	early ( SR, 2 );
	const float*	ch[ 2 ] = { zeros.data (), zeros.data () };
	early.process ( ch, int ( SR * 0.3 ) );
	check ( early.integratedLUFS () == -HUGE_VAL, "silence: below 400 ms reads -inf" );
}
//-----------------------------------------------------------------------------

int main ()
{
	tech3341 ();
	dualMono ();
	chunkInvariance ();
	progressive ();
	silence ();

	printf ( "\n%s (%d failure%s)\n", failures ? "FAILED" : "all tests passed", failures, failures == 1 ? "" : "s" );
	return failures ? 1 : 0;
}
