// Mode-loudness measurement tool (diagnostic, NOT a golden test).
//
// Loads the sidplay_ab_test golden reference WAVs from Tests/reference/sid/
// (raw engine output, no FX chain, so every tune rendered there is usable
// input material and nothing needs rendering here), runs each through all four
// SIDEffects modes (process + applyGlain, so the per-mode fxGains trims are
// included) and measures EBU R128 integrated loudness per mode.
//
// Prints per-tune LUFS plus the per-mode average delta vs MAGIC, and writes
// the same report to Tests/reference/loudness/mode-loudness.txt, so committed
// FX/EQ changes show their loudness drift directly in the diff. Use it as a drift
// ruler, NOT as the source of truth for the fxGains table: that table is tuned
// for equal FEEL and deliberately deviates from equal LUFS where measurement
// and perception disagree (REAL's middy, hissy character reads louder than it
// measures).
//
//   cmake --build --preset vs --config Release --target fx_mode_loudness
//   Builds/vs/Release/fx_mode_loudness.exe

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Audio/PerceivedLoudness.h"
#include "Audio/SIDEffects.h"

#include "test_common.h"

namespace fs = std::filesystem;
using namespace testcommon;

static constexpr int	SR = 44100;
static constexpr int	numModes = 5;
static const char*	modeNames[ numModes ] = { "REAL", "PURE", "MAGIC", "EPIC", "MYTHIC" };

//-----------------------------------------------------------------------------

static double lufs ( const float* L, const float* R, const uint32_t frames )
{
	PerceivedLoudness	ebu ( SR, 2 );
	const float*	channels[ 2 ] = { L, R };
	ebu.process ( channels, static_cast<int>( frames ) );
	return ebu.integratedLUFS ();
}

//-----------------------------------------------------------------------------

static std::string	report;

// print and collect for the report file in one go
static void emit ( const char* fmt, ... )
{
	char	line[ 256 ];
	va_list	args;
	va_start ( args, fmt );
	vsnprintf ( line, sizeof ( line ), fmt, args );
	va_end ( args );

	fputs ( line, stdout );
	report += line;
}

//-----------------------------------------------------------------------------

int main ()
{
	const fs::path	refDir = fs::path ( LOUDNESS_TEST_ROOT ) / "Tests" / "reference" / "sid";
	const fs::path	reportPath = fs::path ( LOUDNESS_TEST_ROOT ) / "Tests" / "reference" / "loudness" / "mode-loudness.txt";

	std::vector<fs::path>	wavs;
	std::error_code	ec;
	for ( const auto& entry : fs::directory_iterator ( refDir, ec ) )
		if ( entry.path ().extension () == ".wav" )
			wavs.push_back ( entry.path () );
	std::sort ( wavs.begin (), wavs.end () );

	if ( wavs.empty () )
	{
		printf ( "no sidplay reference WAVs in %s - run sidplay_ab_test first\n", refDir.string ().c_str () );
		return 1;
	}

	emit ( "%-36s %7s %7s %7s %7s %7s %7s\n", "tune (integrated LUFS)", "input", "REAL", "PURE", "MAGIC", "EPIC", "MYTHIC" );

	double	deltaSum[ numModes ] = {};	// per mode, vs MAGIC
	double	deltaMax[ numModes ] = {};
	auto	numTunes = 0;

	for ( const auto& path : wavs )
	{
		Wav	wav;
		if ( ! readWavF32 ( path, wav ) || ! wav.frames () )
		{
			emit ( "%-36s SKIPPED (unreadable)\n", path.filename ().string ().c_str () );
			continue;
		}

		const auto	frames = wav.frames ();
		const auto	stereo = wav.channels >= 2;

		// deinterleave; mono tunes get a silent right buffer for the in-place chain
		std::vector<float>	inL ( frames ), inR ( frames );
		for ( uint32_t i = 0; i < frames; ++i )
		{
			inL[ i ] = wav.samples[ i * wav.channels ];
			inR[ i ] = stereo ? wav.samples[ i * wav.channels + 1 ] : 0.0f;
		}

		const auto	inputDb = lufs ( inL.data (), stereo ? inR.data () : inL.data (), frames );

		double	modeDb[ numModes ];
		for ( auto mode = 0; mode < numModes; ++mode )
		{
			auto	outL = inL;
			auto	outR = inR;

			// mirror the app's setup: default parameters, no chip bass-adjust,
			// full stereo width, curated stereo placement enabled
			auto	fx = std::make_unique<SIDEffects> ();	// large delay/reverb buffers, keep off the stack
			fx->setFXParameter ( SIDEffects::FXParameter::stereo_processing, 1.0f );
			fx->setChipModel ( false, false, 100, 0.0f );
			fx->setStereo ( stereo );
			fx->setVolume ( 1.0f );
			fx->setFXMode ( mode );
			fx->snapFXTransition ();	// measure the settled mode, not the morph into it

			for ( uint32_t off = 0; off < frames; off += 735 )
			{
				const auto	n = static_cast<int>( std::min<uint32_t> ( 735, frames - off ) );
				float*	b[ 2 ] = { outL.data () + off, outR.data () + off };
				fx->process ( b, n );
				fx->applyGlain ( b, n );
			}

			modeDb[ mode ] = lufs ( outL.data (), outR.data (), frames );
		}

		emit ( "%-36s %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f\n", path.stem ().string ().c_str (),
			   inputDb, modeDb[ 0 ], modeDb[ 1 ], modeDb[ 2 ], modeDb[ 3 ], modeDb[ 4 ] );

		for ( auto mode = 0; mode < numModes; ++mode )
		{
			const auto	delta = modeDb[ mode ] - modeDb[ SIDEffects::FXMode::MAGIC ];
			deltaSum[ mode ] += delta;
			deltaMax[ mode ] = std::max ( deltaMax[ mode ], std::fabs ( delta ) );
		}
		++numTunes;
	}

	if ( ! numTunes )
		return 1;

	emit ( "\naverage delta vs MAGIC (positive = measures louder), %d tunes:\n", numTunes );
	for ( const auto mode : { 0, 1, 3, 4 } )
		emit ( "  %-5s %+5.1f dB  (worst tune %+.1f dB)\n", modeNames[ mode ], deltaSum[ mode ] / numTunes, deltaMax[ mode ] );

	emit ( "\nnote: fxGains is tuned for equal FEEL; use these numbers to spot drift\n"
		   "after FX/EQ changes, not as a target to equalize against\n" );

	fs::create_directories ( reportPath.parent_path (), ec );
	std::ofstream	f ( reportPath, std::ios::trunc );
	f << report;
	printf ( "\nreport written to %s\n", reportPath.string ().c_str () );
	return 0;
}
