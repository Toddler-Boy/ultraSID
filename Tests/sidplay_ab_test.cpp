// Golden-output regression + performance test for libSidplayEZ.
//
// Tunes are listed in Tests/tunes/tunes.txt, each prefixed with the root it
// lives under ("$HVSC$/MUSICIANS/..." , "$EXOT$/..."), and played from where
// they really live, so the engine's path-matched chip/audio profiles and tune
// overrides apply exactly as in the app (default subtune, 30 s @ 44.1 kHz,
// full app config: sidid, chip/audio profiles, tune overrides, ROMs).
// Tests/data-roots.txt (machine-specific, keep untracked) says where each
// marker points, as "$HVSC$ = E:/documents/C64Music", the same markers
// ultraSID uses in its stored keys. A relative root resolves against the repo,
// so "$EXOT$ = Data/Exotic tunes" works on any checkout. The first
// command-line argument, when given, overrides $HVSC$.
// No copies of any .sid live under Tests/.
// Tunes listed in Tests/tunes/not-filtered.txt (one name per line, '#' for
// comments) render with the SID filter disabled, mirroring the app's
// per-tune database setting, so both the filter and the templated no-filter
// path get coverage. Everything else renders with the filter on.
//  - first run per tune: writes a float-WAV reference into Tests/reference/sid/
//    and records a performance baseline
//  - later runs: compares the fresh render against the reference (the engine
//    is fully deterministic, fixed seeds everywhere, so unchanged code
//    reproduces it bit-exactly) and reports render speed vs the baseline
//
// Exit 0: all tunes match (below -80 dB relative peak; bit-exact is reported
//         as such) and no performance regression.
// Exit 1: audio above -80 dB relative peak, length/channel mismatch, a tune
//         failed to load, or no tunes found.
// Exit 2: audio fine, but performance regressed. Thermal/boost state shifts
//         ALL tunes together by up to ~±15 % between runs, so the median
//         delta across tunes is treated as the machine factor; a tune is
//         flagged when it stays >10 % above that median after a confirming
//         re-render (path-specific regression), or when the median itself
//         exceeds +25 % (uniform slowdown beyond any plausible machine state).
// To re-baseline after an intentional change: delete the matching file(s) in
// Tests/reference/sid/ (audio: <tune>.wav, performance: perf-baseline.txt lines).
//
//   cmake --build --preset vs --config Release --target sidplay_ab_test
//   Builds/vs/Release/sidplay_ab_test.exe [dataRoot] [seconds]

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "libSidplayEZ/src/EZ/player.h"

#include "test_common.h"

namespace fs = std::filesystem;
using namespace testcommon;

//-----------------------------------------------------------------------------

int main ( int argc, char** argv )
{
	const fs::path	root = SIDPLAY_TEST_ROOT;
	const fs::path	tunesDir = root / "Tests" / "tunes";
	const int	seconds = argc > 2 ? std::atoi ( argv[ 2 ] ) : 30;

	const auto	trimLine = [] ( std::string& line )
	{
		while ( !line.empty () && ( line.back () == '\r' || line.back () == ' ' || line.back () == '\t' ) )
			line.pop_back ();
	};

	// Roots by marker, e.g. "$HVSC$ = E:/documents/C64Music". The same markers ultraSID uses
	// in its stored keys. A relative root resolves against the repo
	std::map<std::string, fs::path>	dataRoots;
	{
		std::istringstream	f ( slurp ( root / "Tests" / "data-roots.txt" ) );
		std::string	line;
		while ( std::getline ( f, line ) )
		{
			trimLine ( line );

			const auto	eq = line.find ( '=' );
			if ( line.empty () || line[ 0 ] != '$' || eq == std::string::npos )
				continue;

			auto	marker = line.substr ( 0, eq );
			auto	dir = line.substr ( eq + 1 );

			trimLine ( marker );
			while ( ! dir.empty () && ( dir.front () == ' ' || dir.front () == '\t' ) )
				dir.erase ( 0, 1 );

			const fs::path	path { dir };
			dataRoots[ marker ] = path.is_absolute () ? path : root / path;
		}
	}

	// A root passed on the command line stands in for the HVSC one
	if ( argc > 1 )
		dataRoots[ "$HVSC$" ] = argv[ 1 ];
	const fs::path	refDir = root / "Tests" / "reference" / "sid";
	const fs::path	baselinePath = refDir / "perf-baseline.txt";
	const int	SR = 44100;

	std::error_code	ec;
	fs::create_directories ( tunesDir, ec );
	fs::create_directories ( refDir, ec );

	// full app config so the render matches what ultraSID plays
	auto	config = std::make_shared<libsidplayEZ::SharedPlayerConfig> ();
	config->loadSidIDConfig ( ( root / "Data" / "sidid.cfg" ).string ().c_str () );
	config->loadChipProfiles ( slurp ( root / "Data" / "Databases" / "chip-profiles.csv" ) );
	config->loadAudioProfiles ( slurp ( root / "Data" / "Databases" / "audio-profiles.csv" ) );
	config->loadTuneOverrides ( slurp ( root / "Data" / "Databases" / "tune-overrides.csv" ) );

	const auto	kernal = slurpBytes ( root / "Data" / "Roms" / "kernal.bin" );
	const auto	basic = slurpBytes ( root / "Data" / "Roms" / "basic.bin" );
	const auto	character = slurpBytes ( root / "Data" / "Roms" / "character.bin" );

	struct TuneRef
	{
		fs::path	file;
		std::string	name;
		bool	found;
	};
	std::vector<TuneRef>	tunes;

	// Tunes are played from where they really live, so the engine's path-matched chip and
	// audio profiles and tune overrides apply exactly as in the app
	{
		std::istringstream	f ( slurp ( tunesDir / "tunes.txt" ) );
		std::string	line;
		while ( std::getline ( f, line ) )
		{
			trimLine ( line );
			if ( line.empty () || line[ 0 ] != '$' )
				continue;

			const auto	markerEnd = line.find ( '$', 1 );
			if ( markerEnd == std::string::npos )
				continue;

			const auto	marker = line.substr ( 0, markerEnd + 1 );

			auto	rest = line.substr ( markerEnd + 1 );
			if ( ! rest.empty () && rest.front () == '/' )
				rest.erase ( 0, 1 );

			const auto	it = dataRoots.find ( marker );

			auto	file = it != dataRoots.end () ? it->second / rest : fs::path { rest };
			auto	name = file.stem ().string ();
			auto	found = it != dataRoots.end () && fs::exists ( file );

			tunes.push_back ( { std::move ( file ), std::move ( name ), found } );
		}
	}

	std::sort ( tunes.begin (), tunes.end (), [] ( const TuneRef& a, const TuneRef& b ) { return a.name < b.name; } );

	// tunes rendered with the SID filter off (mirrors the app's database setting)
	std::vector<std::string>	noFilter;
	{
		std::istringstream	f ( slurp ( tunesDir / "not-filtered.txt" ) );
		std::string	line;
		while ( std::getline ( f, line ) )
		{
			while ( !line.empty () && ( line.back () == '\r' || line.back () == ' ' || line.back () == '\t' ) )
				line.pop_back ();
			if ( line.empty () || line[ 0 ] == '#' )
				continue;
			if ( line.size () > 4 && line.ends_with ( ".sid" ) )
				line.resize ( line.size () - 4 );
			std::transform ( line.begin (), line.end (), line.begin (), [] ( unsigned char c ) { return char ( std::tolower ( c ) ); } );
			noFilter.push_back ( line );
		}
	}
	const auto	usesFilter = [ & ] ( const fs::path& tunePath )
	{
		auto	stem = tunePath.stem ().string ();
		std::transform ( stem.begin (), stem.end (), stem.begin (), [] ( unsigned char c ) { return char ( std::tolower ( c ) ); } );
		return std::find ( noFilter.begin (), noFilter.end (), stem ) == noFilter.end ();
	};

	if ( tunes.empty () )
	{
		printf ( "no tunes found - drop .sid files into %s\n", tunesDir.string ().c_str () );
		return 1;
	}

	auto	baseline = readBaseline ( baselinePath );
	auto	baselineDirty = false;
	auto	audioFail = false;
	auto	perfFail = false;

	struct PerfInfo
	{
		fs::path	path;
		std::string	name;
		bool	filter;
		double	delta;
	};
	std::vector<PerfInfo>	perfInfos;

	struct Render
	{
		bool	ok = false;
		bool	stereo = false;
		double	elapsed = 0.0;
		std::vector<float>	L, R;
	};

	const auto	renderTune = [ & ] ( const fs::path& tunePath, const bool filter ) -> Render
	{
		Render	r;

		// one Player per tune/render, never reused across tunes
		auto	playerPtr = std::make_unique<libsidplayEZ::Player> ();
		auto&	player = *playerPtr;
		player.setSharedConfig ( config );
		player.setRoms ( kernal.empty () ? nullptr : kernal.data (),
						 basic.empty () ? nullptr : basic.data (),
						 character.empty () ? nullptr : character.data () );
		player.setSamplerate ( SR );

		if ( !player.loadSidFile ( tunePath.string ().c_str () ) || !player.setTuneNumber ( 0, filter ) || !player.isReadyToPlay () )
			return r;

		r.stereo = player.getNumChips () >= 2;
		const auto	total = static_cast<uint32_t>( SR ) * static_cast<uint32_t>( seconds );
		r.L.resize ( total );
		if ( r.stereo )
			r.R.resize ( total );

		// per-chip digi buffers are mandatory, and a 4E tune can ask for any number of chips
		constexpr uint32_t	kRenderChunk = 735;	// 44100/60, must stay > 100 (Mixer::begin assert)
		const auto	numChips = size_t ( player.getNumChips () );

		std::vector<int8_t>				digi ( numChips * kRenderChunk );
		std::vector<std::span<int8_t>>	digiPtrs ( numChips );

		for ( size_t i = 0; i < numChips; ++i )
			digiPtrs[ i ] = { digi.data () + i * kRenderChunk, kRenderChunk };

		const auto	t0 = std::chrono::steady_clock::now ();
		for ( uint32_t done = 0; done < total; )
		{
			const auto	want = std::min ( kRenderChunk, total - done );
			const auto	got = player.runEmulation ( { r.L.data () + done, want },
													r.stereo ? std::span<float> { r.R.data () + done, want } : std::span<float> {},
													digiPtrs );
			if ( got != want )
				return r;
			done += got;
		}
		r.elapsed = std::chrono::duration<double> ( std::chrono::steady_clock::now () - t0 ).count ();
		r.ok = true;
		return r;
	};

	// peak diff / peak signal between two same-length float buffers
	const auto	comparePeaks = [] ( const std::vector<float>& a, const std::vector<float>& b, double& peakDiff, double& peakSig )
	{
		for ( size_t i = 0; i < a.size (); ++i )
		{
			peakDiff = std::max ( peakDiff, std::fabs ( static_cast<double>( a[ i ] ) - b[ i ] ) );
			peakSig = std::max ( peakSig, static_cast<double>( std::fabs ( a[ i ] ) ) );
		}
	};

	for ( const auto& tune : tunes )
	{
		const auto&	tunePath = tune.file;
		const auto&	name = tune.name;
		const auto	filter = usesFilter ( tunePath );
		printf ( "%-40s %-10s ", name.c_str (), filter ? "[filter]" : "[nofilter]" );

		if ( ! tune.found )
		{
			printf ( "FAIL (not found - check its root in Tests/data-roots.txt)\n" );
			audioFail = true;
			continue;
		}

		const auto	rnd = renderTune ( tunePath, filter );
		if ( !rnd.ok )
		{
			printf ( "FAIL (could not load/init/render)\n" );
			audioFail = true;
			continue;
		}

		const auto	total = static_cast<uint32_t>( rnd.L.size () );
		const auto	stereo = rnd.stereo;
		const auto&	outL = rnd.L;
		const auto&	outR = rnd.R;
		const auto	elapsed = rnd.elapsed;
		const auto	speed = seconds / elapsed;
		const auto	refPath = refDir / ( name + ".wav" );

		if ( !fs::exists ( refPath ) )
		{
			if ( !writeWavF32 ( refPath, outL.data (), stereo ? outR.data () : nullptr, total, SR ) )
			{
				printf ( "FAIL (could not write reference)\n" );
				audioFail = true;
				continue;
			}
			baseline[ name ] = { seconds, elapsed };
			baselineDirty = true;

			// a golden reference is only meaningful if the render reproduces:
			// render again with a completely fresh Player and verify
			const auto	rnd2 = renderTune ( tunePath, filter );
			auto	pd = 0.0, ps = 0.0;
			if ( rnd2.ok && rnd2.stereo == stereo )
			{
				comparePeaks ( outL, rnd2.L, pd, ps );
				if ( stereo )
					comparePeaks ( outR, rnd2.R, pd, ps );
			}
			char	repeatTxt[ 64 ];
			if ( !rnd2.ok || rnd2.stereo != stereo )
				snprintf ( repeatTxt, sizeof ( repeatTxt ), "repeat render FAILED" );
			else if ( pd == 0.0 )
				snprintf ( repeatTxt, sizeof ( repeatTxt ), "repeat bit-exact" );
			else
				snprintf ( repeatTxt, sizeof ( repeatTxt ), "REPEAT DIFFERS %.1f dB", 20.0 * std::log10 ( pd / std::max ( ps, 1e-30 ) ) );

			printf ( "reference created  (%s, %.1fx realtime, %s)\n", stereo ? "stereo" : "mono", speed, repeatTxt );
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
		const auto	channels = stereo ? 2 : 1;
		if ( ref.channels != channels || ref.frames () != total )
		{
			printf ( "FAIL (layout changed: ref %dch/%u frames vs now %dch/%u - delete the reference to re-baseline)\n",
					 ref.channels, ref.frames (), channels, total );
			audioFail = true;
			continue;
		}

		auto	peakDiff = 0.0, peakSig = 0.0;
		auto	firstDiff = -1.0;
		auto	maxDiffAt = -1.0;
		for ( uint32_t i = 0; i < total; ++i )
		{
			for ( auto c = 0; c < channels; ++c )
			{
				const double	r = ref.samples[ static_cast<size_t>( i ) * channels + c ];
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

		// --- performance vs baseline (verdict deferred: the machine factor is
		// the median delta over all tunes, only computable after the loop)
		char	perfTxt[ 96 ];
		const auto	it = baseline.find ( name );
		if ( it != baseline.end () && it->second.first == seconds )
		{
			const auto	delta = ( elapsed / it->second.second - 1.0 ) * 100.0;
			perfInfos.push_back ( { tunePath, name, filter, delta } );
			snprintf ( perfTxt, sizeof ( perfTxt ), "%.1fx realtime, %+.1f%% vs baseline", speed, delta );
		}
		else
		{
			baseline[ name ] = { seconds, elapsed };
			baselineDirty = true;
			snprintf ( perfTxt, sizeof ( perfTxt ), "%.1fx realtime, baseline recorded", speed );
		}

		printf ( "%s  %-14s  %s\n", audioOK ? "ok  " : "FAIL", audioTxt, perfTxt );
	}

	// --- performance verdict: median delta = machine factor (thermal/boost
	// state moves all tunes together); flag per-tune deviations above it,
	// confirmed by a re-render, and a uniform slowdown beyond machine range
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

			// suspected path-specific regression: confirm with a re-render
			const auto	retry = renderTune ( p.path, p.filter );
			const auto	it = baseline.find ( p.name );
			auto	confirmed = p.delta;
			if ( retry.ok && it != baseline.end () )
				confirmed = std::min ( confirmed, ( retry.elapsed / it->second.second - 1.0 ) * 100.0 );
			if ( confirmed - median > 10.0 )
			{
				perfFail = true;
				printf ( "PERF REGRESSION: %s %+.1f%% vs baseline (%+.1f%% above machine factor, re-render confirmed)\n",
						 p.name.c_str (), confirmed, confirmed - median );
			}
		}

		if ( median > 25.0 )
		{
			perfFail = true;
			printf ( "PERF REGRESSION: uniform slowdown of %+.1f%% across all tunes - beyond machine-state range\n", median );
		}
	}

	if ( baselineDirty )
		writeBaseline ( baselinePath, baseline );

	printf ( "\n%s\n", audioFail ? "FAIL (audio)" : perfFail ? "PASS audio, FAIL performance" : "PASS" );
	return audioFail ? 1 : perfFail ? 2 : 0;
}
