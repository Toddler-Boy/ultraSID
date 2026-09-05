#include <algorithm>
#include <array>
#include <format>
#include <set>
#include <span>
#include <vector>

#include "sid_scanner.h"

#include "libSidplayEZ/src/EZ/dsp-downmix.h"
#include "libSidplayEZ/src/EZ/dsp-subsonic-filter.h"

#include "ultra-shared/Config/YamlFile.h"

#include "Audio/PerceivedLoudness.h"

//-----------------------------------------------------------------------------

juce::File ultraSIDHVSCPath ()
{
	const auto	settingsFile = juce::File::getSpecialLocation ( juce::File::SpecialLocationType::userApplicationDataDirectory )
							.getChildFile ( "ultraSID" ).getChildFile ( "settings.yml" );

	const YamlFile	settings ( { { "paths", "hvsc", juce::SystemStats::getEnvironmentVariable ( "HVSC_BASE", juce::File::getSpecialLocation ( juce::File::SpecialLocationType::userDocumentsDirectory ).getChildFile ( "C64Music" ).getFullPathName () ).toStdString () } }, settingsFile, true );

	return juce::File ( settings.get<juce::String> ( "paths/hvsc" ) );
}
//-----------------------------------------------------------------------------

void scannerLoadBytes ( const char* fileName, std::vector<uint8_t>& bufferRef )
{
	// Absolute = the Exotic-tunes mirror, anything else is collection-relative
	if ( ! juce::File::isAbsolutePath ( fileName ) )
	{
		hvscsource::loadBytes ( fileName, bufferRef );
		tunepatches::apply ( fileName, bufferRef );
		return;
	}

	juce::MemoryBlock	mb;
	juce::File ( fileName ).loadFileAsData ( mb );

	const auto*	data = static_cast<const uint8_t*> ( mb.getData () );
	bufferRef.assign ( data, data + mb.getSize () );
}
//-----------------------------------------------------------------------------

// The ROMs come from ultraSID's Data folder, the tool carries no duplicates
static juce::MemoryBlock loadRom ( const char* name )
{
	juce::MemoryBlock	rom;

	dataRoot ().getChildFile ( "Roms" ).getChildFile ( name ).loadFileAsData ( rom );
	jassert ( rom.getSize () > 0 );

	return rom;
}
//-----------------------------------------------------------------------------

MeasureLoudness::MeasureLoudness ( std::shared_ptr<const libsidplayEZ::SharedPlayerConfig> config )
{
	engine.setSharedConfig ( std::move ( config ) );

	engine.setSamplerate ( 44100 );

	// The blocks outlive every engine regardless of whether setRoms copies
	static const auto	kernal = loadRom ( "kernal.bin" );
	static const auto	basic = loadRom ( "basic.bin" );
	static const auto	chargen = loadRom ( "chargen.bin" );

	engine.setRoms ( kernal.getData (), basic.getData (), chargen.getData () );
}
//-----------------------------------------------------------------------------

// FNV-1a 64-bit as 16 hex chars; a change detector, not cryptography
static std::string settingsHash16 ( const std::string& text )
{
	auto	hash = 14695981039346656037ull;

	for ( const unsigned char c : text )
	{
		hash ^= c;
		hash *= 1099511628211ull;
	}

	return std::format ( "{:016x}", hash );
}
//-----------------------------------------------------------------------------

// The racing registers name the school: multiple voices mean resynthesis,
// a single voice's registers pick the capture mode (its ctrl writes are the
// technique's clock, not a signal of their own); $D418 rides along freely
static std::string deriveDigiHint ( const std::set<int>& racing )
{
	std::set<int>	core = racing;
	core.erase ( 0x18 );

	if ( core.empty () )
		return {};

	if ( core == std::set<int> { 0x17 } )
		return "filt1";

	std::set<int>	voices;
	for ( const auto reg : core )
		if ( reg < 0x15 )
			voices.insert ( reg / 7 );

	if ( voices.size () != 1 )
		return voices.size () > 1 ? "output (resynthesis)" : "unclear";

	const auto	v = *voices.begin ();
	const auto	n = std::to_string ( v + 1 );
	const auto	freqLo = 7 * v, freqHi = 7 * v + 1, pwLo = 7 * v + 2, pwHi = 7 * v + 3, ctrl = 7 * v + 4;

	auto	kinds = core;
	kinds.erase ( ctrl );

	if ( kinds.empty () )
		return std::array { "covox", "ctrl2 stream (no mode yet)", "voice3Out or escos" } [ size_t ( v ) ];

	if ( std::ranges::includes ( std::set<int> { freqLo, freqHi }, kinds ) )
	{
		if ( kinds.contains ( freqLo ) )
			return "freq" + n + " lo-byte latch (no mode yet)";
		return v == 0 ? "freq1 or carmina" : "freq" + n;
	}
	if ( kinds == std::set<int> { pwHi } )
		return v == 0 ? "voice1Pwm or pwHi1" : "pwHi" + n + " (no mode yet)";
	if ( kinds == std::set<int> { pwLo } )
		return v == 0 ? "pwLo1" : "pwLo" + n + " (no mode yet)";
	if ( kinds == std::set<int> { pwLo, pwHi } )
		return v == 0 ? "pwFull1" : "pwFull" + n + " (no mode yet)";

	return "unclear";
}
//-----------------------------------------------------------------------------

MeasureLoudness::result MeasureLoudness::measureTune ( const char* filename, const int tuneNo, const uint32_t renderLengthMs, const bool useFilter, const std::string& storedSettingsHash, const bool force6581, const bool force8580, std::atomic<uint32_t>* renderedMs, std::atomic<uint64_t>* speedSample, std::atomic<uint8_t>* featureFlags, const std::atomic<bool>* abortFlag )
{
	auto setFeature = [ featureFlags ] ( const uint8_t flag )
	{
		if ( featureFlags )
			featureFlags->fetch_or ( flag, std::memory_order_relaxed );
	};

	constexpr auto	sixtyHzLength = 44100u / 60u;

	auto	lengthWanted = uint32_t ( ( (uint64_t)renderLengthMs * 44100ull ) / 1000ull );	// convert to number of samples at 44.1kHz

	// Round up to next block-size
	lengthWanted = uint32_t ( ( ( (uint64_t)lengthWanted + sixtyHzLength - 1 ) / sixtyHzLength ) * sixtyHzLength );

	// Loop detection: render extra time past the song length and look for signal
	// in the last loopTailBlocks, the blocks before that are grace time for the
	// final note to decay (20s extra, 18s tail, 2s grace). A loop is half a
	// second of UNINTERRUPTED signal anywhere in the tail: a restarting song
	// plays continuously, while the rare stray pops one-shot speech/FX tunes
	// emit past the grace window are single isolated blocks. Loudness and digi
	// analysis cover only the song length itself
	constexpr auto	loopExtraBlocks = 20u * 60u;
	constexpr auto	loopTailBlocks = 18u * 60u;
	constexpr auto	loopRunBlocksNeeded = 30u;

	// Start-of-song detection: the start is the first active block after a short guard
	// window at capture start. The guard swallows the capture-start micro-pop (ring of the
	// init routine's volume pokes, which happen before the declick can arm; Shades has one
	// ~-44 dBFS block at capture start, then silence until the music at 4.27s).
	// The pop can only occur at capture start, and any start below one second is reported
	// as 0:00 anyway, so a guard below 60 blocks cannot shift any result. Past the guard
	// even a single-block burst counts: tunes opening with short bursts would otherwise
	// report their start far too late.
	// The stored offset backs off by a small lead-in, so playback doesn't cut in exactly
	// on the first audible sample
	constexpr auto	startGuardBlocks = 30u;	// 0.5s
	constexpr auto	startLeadInMs = 100u;

	auto	musicBlocks = lengthWanted / sixtyHzLength;	// shifts when a delayed start is detected

	lengthWanted += loopExtraBlocks * sixtyHzLength;

	if ( ! engine.loadSidFile ( scannerLoadBytes, filename ) )
		return { "loadSidFile failed", -96.0f, -96.0f, true, true };

	if ( ! engine.setTuneNumber ( tuneNo, useFilter ) )
		return { "setTuneNumber failed", -96.0f, -96.0f, true, true };

	if ( engine.isJammed () )
		return { "HLT", -96.0f, -96.0f, true, true };

	const auto& info = engine.getFileInfo ();
	const auto	numChips = engine.getNumChips ();

	Z_DLOG ( juce::String ( filename ) << " tune " << tuneNo << ": width " << info.stereoWidth << ", profile \"" << juce::String ( info.chipProfile ) << "\"" );

	// Settings fingerprint over everything that shapes this subtune's output.
	// The chip part collapses to a marker where profile values can't matter
	// (emu editors pin their own settings, 8580s have no profiles); elsewhere
	// it is the applied non-default values, so unprofiled tunes serialize
	// empty and new transparent-by-default fields change nothing
	const auto	has6581 = std::find ( info.model.begin (), info.model.end (), "6581" ) != info.model.end ();

	std::string	fingerprint;
	{
		std::string	chipPart;

		if ( info.chipProfile.starts_with ( "emu-" ) )
			chipPart = info.chipProfile;
		else if ( ! has6581 )
			chipPart = "8580";
		else
			chipPart = info.chipSettingsValues;

		auto	text = chipPart + "|";
		for ( const auto& model : info.model )
			text += model + ",";
		text += "|" + info.clock + "|" + std::to_string ( info.stereoWidth ) + "|" + info.md5 + "|" + std::to_string ( renderLengthMs );

		fingerprint = settingsHash16 ( text );
	}

	// A forced re-measure is model-scoped: the 8580 emulation effectively
	// never changes, so only force8580 touches tunes without a 6581
	if ( ! storedSettingsHash.empty () && fingerprint == storedSettingsHash && ! ( has6581 ? force6581 : force8580 ) )
		return { .settingsHash = fingerprint, .settingsMatch = true };

	const auto	isStereo = numChips > 1;

	PerceivedLoudness	ebu ( 44100.0, isStereo ? 2 : 1 );

	// Digi detection, gating the master-volume display: a regular tune's digi
	// buffer only moves with rare, slow master-volume work (a fade steps at
	// most once per block), a sample player changes it dozens of times per
	// block. Enough dense blocks = digi, however long the song
	constexpr auto	activeBlockChanges = 4;
	constexpr auto	digiBlocksNeeded = 10u;

	constexpr auto	silentLevel = 1e-3f;	// -60 dBFS block peak counts as silent

	const auto	analyzeDigi = info.digiPlayer || info.c64PlayAddress == 0;

	// Rate detection wants raw level changes, not display conditioning
	engine.setDigiSmoothing ( false );

	// The computed display modes move with any music, useless for rate
	// detection: watch the raw write stream of the technique's register instead,
	// which only races during sample playback. Tunes without an established
	// verdict scan in unknown mode: the buffer keeps the plain nibble level
	// (wiggler detection intact) and every register's write rate is counted.
	// Covered tunes (including "none" rows) skip that heuristic for good
	const auto	scanMode = info.digiCovered ? info.digiMode : reSIDfp::DigiMode::unknown;
	engine.setDigiScan ( scanMode );

	// One digi buffer per chip, a fixed [ 3 ] would overflow on 4E tunes
	const auto	chipCount = size_t ( numChips );

	std::vector<std::vector<int8_t>>	digiBuffers ( chipCount, std::vector<int8_t> ( sixtyHzLength ) );
	std::vector<std::span<int8_t>>		digiSpans ( digiBuffers.begin (), digiBuffers.end () );

	std::vector<int>	lastVal ( chipCount, INT_MIN );
	auto		digiConfirmed = false;
	uint32_t	digiActiveBlocks = 0;
	uint32_t	tailActiveBlocks = 0;
	uint32_t	blockIndex = 0;
	int			startBlock = -1;	// first active block past the capture-start guard

	auto	filterUsed = false;
	auto	tuneEnded = false;		// the CPU jammed mid-render, see the render loop

	// Peak sample-to-sample change of the current block, immune to lingering
	// DC offsets and the DC-blocker's slow settle after the last note
	auto blockActivity = [ & ] ( const int numSamples )
	{
		auto	activity = 0.0f;

		for ( auto i = 1; i < numSamples; ++i )
			activity = std::max ( activity, std::abs ( outBufferL[ i ] - outBufferL[ i - 1 ] ) );

		if ( isStereo )
			for ( auto i = 1; i < numSamples; ++i )
				activity = std::max ( activity, std::abs ( outBufferR[ i ] - outBufferR[ i - 1 ] ) );

		return activity;
	};

	libsidplayEZ::dsp::SubsonicFilter	subsonicFilter[ 2 ];

	// Publishes rendered-audio/wall-time as one consistent pair, so the reader
	// can never see a numerator and denominator from different moments
	const auto	wallStart = juce::Time::getMillisecondCounterHiRes ();

	auto publishSpeedSample = [ & ]
	{
		if ( ! speedSample )
			return;

		const auto	audioMs = uint64_t ( blockIndex ) * sixtyHzLength * 1000ull / 44100ull;
		const auto	wallMs = uint64_t ( juce::Time::getMillisecondCounterHiRes () - wallStart );

		speedSample->store ( ( audioMs << 32 ) | ( wallMs & 0xffffffffull ), std::memory_order_relaxed );
	};

	while ( lengthWanted )
	{
		if ( abortFlag && abortFlag->load ( std::memory_order_relaxed ) )
			return { "aborted", -96.0f, -96.0f, true, true };

		const auto	requestLength = std::min ( lengthWanted, sixtyHzLength );	// generate only up to 60Hz worth of data
		{
			const auto	wordsWritten = engine.runEmulation ( std::span ( outBufferL, requestLength ), isStereo ? std::span<float> ( outBufferR, requestLength ) : std::span<float> (), digiSpans );

			// Some rips end by crashing the CPU (a BRK into the psid driver's halt
			// trap): everything rendered so far is the tune, and a dead CPU can't
			// loop. Process the partial block, then finish with a one-shot verdict
			tuneEnded = engine.isJammed ();

			subsonicFilter[ 0 ].process ( outBufferL, wordsWritten );
			if ( isStereo )
				subsonicFilter[ 1 ].process ( outBufferR, wordsWritten );

			if ( blockIndex < musicBlocks )
			{
				if ( isStereo )
					libsidplayEZ::dsp::downMix ( outBufferL, outBufferR, wordsWritten, info.stereoWidth * 0.01f );

				const float*	ebuChannels[ 2 ] = { outBufferL, isStereo ? outBufferR : nullptr };
				ebu.process ( ebuChannels, wordsWritten );

				// Filter detection
				if ( ! filterUsed )
				{
					for ( auto chip = 0; chip < numChips; ++chip )
					{
						uint8_t	regs[ 32 ];
						if ( engine.getSidStatus ( chip, regs ) )
						{
							// A voice routed into the filter is shaped or, with no mode
							// selected, muted; an engaged filter with nothing routed is inaudible
							filterUsed = ( regs[ 0x17 ] & 0x07 ) != 0;

							if ( filterUsed )
							{
								// Rendering filter-less on a stale verdict, bail for a filtered re-render
								if ( ! useFilter )
									return { .filterMismatch = true };

								setFeature ( featFilter );
								break;
							}
						}
					}
				}

				// Start-of-song detection: first active block past the capture-start guard
				if ( startBlock < 0 && blockIndex >= startGuardBlocks )
				{
					if ( blockActivity ( wordsWritten ) > silentLevel )
					{
						startBlock = int ( blockIndex );

						// Whether the start counts as delayed is decided right here
						if ( startBlock >= 60 )
						{
							setFeature ( featDelayedStart );

							// The songlength ignores the silent intro, so the music
							// window (and with it the loop tail) shifts by it too
							musicBlocks += uint32_t ( startBlock );
							lengthWanted += uint32_t ( startBlock ) * sixtyHzLength;
						}
					}
				}

				// A block with dense digi-buffer changes marks sample playback
				if ( analyzeDigi && ! digiConfirmed )
				{
					auto	blockIsActive = false;

					for ( auto chip = size_t ( 0 ); chip < size_t ( numChips ); ++chip )
					{
						auto	curVal = lastVal[ chip ];
						if ( curVal == INT_MIN )
							curVal = digiBuffers[ chip ][ 0 ];

						auto	changes = 0;
						for ( auto i = 0; i < wordsWritten; ++i )
						{
							const auto	val = int ( digiBuffers[ chip ][ i ] );
							if ( val != curVal )
							{
								curVal = val;
								++changes;
							}
						}

						lastVal[ chip ] = curVal;
						blockIsActive |= changes >= activeBlockChanges;
					}

					digiActiveBlocks += blockIsActive;

					// The count only grows: at the threshold the verdict is
					// final, flag it live and skip further scanning
					if ( digiActiveBlocks >= digiBlocksNeeded )
					{
						digiConfirmed = true;
						setFeature ( featDigi );
					}
				}
			}
			else if ( blockIndex >= musicBlocks + ( loopExtraBlocks - loopTailBlocks ) )
			{
				// Loop-detection tail: a sustained run of active blocks is the song
				// restarting. Stop rendering as soon as the verdict can't change
				// anymore: run complete means looping, too few blocks left to ever
				// complete one means the tune ended for good
				if ( blockActivity ( wordsWritten ) > silentLevel )
					++tailActiveBlocks;
				else
					tailActiveBlocks = 0;

				if ( tailActiveBlocks >= loopRunBlocksNeeded )
					break;

				const auto	blocksLeft = musicBlocks + loopExtraBlocks - 1 - blockIndex;
				if ( tailActiveBlocks + blocksLeft < loopRunBlocksNeeded )
					break;
			}

			++blockIndex;
			lengthWanted -= wordsWritten;

			if ( renderedMs )
				renderedMs->store ( uint32_t ( uint64_t ( blockIndex ) * sixtyHzLength * 1000ull / 44100ull ), std::memory_order_relaxed );

			if ( ( blockIndex % 600 ) == 0 )
				publishSpeedSample ();

			if ( tuneEnded )
				break;
		}
	}

	publishSpeedSample ();

	// Unknown-mode scans report every register written at digi rates, the
	// evidence for techniques nobody mapped yet (chips merge by max, stereo
	// digis are not a thing)
	std::string	writeRates;
	std::string	digiHint;

	if ( scanMode == reSIDfp::DigiMode::unknown && analyzeDigi )
	{
		reSIDfp::DigiCapture::WriteRates	merged;

		for ( auto chip = 0; chip < numChips; ++chip )
		{
			reSIDfp::DigiCapture::WriteRates	rates;
			if ( ! engine.getDigiWriteRates ( chip, rates ) )
				continue;

			for ( auto reg = 0; reg < reSIDfp::DigiCapture::watchedRegs; ++reg )
			{
				merged.maxPerBlock[ reg ] = std::max ( merged.maxPerBlock[ reg ], rates.maxPerBlock[ reg ] );
				merged.busyBlocks[ reg ] = std::max ( merged.busyBlocks[ reg ], rates.busyBlocks[ reg ] );
			}
		}

		std::set<int>	racing;
		for ( auto reg = 0; reg < reSIDfp::DigiCapture::watchedRegs; ++reg )
			if ( merged.busyBlocks[ reg ] >= digiBlocksNeeded )
			{
				racing.insert ( reg );
				writeRates += std::format ( "{}{:02X}:{}/{}", writeRates.empty () ? "" : ",", reg, merged.maxPerBlock[ reg ], merged.busyBlocks[ reg ] );
			}

		digiHint = deriveDigiHint ( racing );

		// A clean unknown scan is a result too: "-" records "scanned, no
		// digi-rate registers", proving the tune clean for the triage tools
		if ( writeRates.empty () )
			writeRates = "-";
	}

	// A register racing at digi rates is a digi whatever school it rides,
	// same doctrine as the buffer check; flagging right away puts the tune
	// in front of ultraSID's eyes without waiting for a mode assignment
	const auto	digiUsed = ( analyzeDigi && digiActiveBlocks >= digiBlocksNeeded ) || ( ! writeRates.empty () && writeRates != "-" );

	const auto	looped = ! tuneEnded && tailActiveBlocks >= loopRunBlocksNeeded;

	// Start offset: audio beginning within the first second (or not at all) counts as 0:00,
	// otherwise back off by the lead-in (the offset is >= 1000 ms here, so this can't wrap)
	const auto	startMs = startBlock >= 60
		? uint32_t ( uint64_t ( startBlock ) * sixtyHzLength * 1000ull / 44100ull ) - startLeadInMs
		: 0u;

	// Final feature pass for what the early checks couldn't decide
	if ( digiUsed )
		setFeature ( featDigi );
	if ( ! looped )
		setFeature ( featOneShot );

	auto	integrated = static_cast<float> ( ebu.integratedLUFS () );

	// If integrated loudness is less than -96 dB, return default -14 dB
	if ( integrated < -96.0f )
		integrated = -14.0f;

	// Floored at -95: that value means "measured, silent midband", while -96.0
	// stays the never-measured marker the re-scan skip logic relies on
	const auto	midLoudness = std::max ( -95.0f, static_cast<float> ( ebu.midLUFS () ) );

	const auto	jammedAtMs = tuneEnded ? uint32_t ( uint64_t ( blockIndex ) * sixtyHzLength * 1000ull / 44100ull ) : 0u;

	result	res { "", integrated, midLoudness, filterUsed, digiUsed, looped, startMs, jammedAtMs, false, fingerprint };
	res.writeRates = writeRates;
	res.digiHint = digiHint;

	return res;
}
//-----------------------------------------------------------------------------
