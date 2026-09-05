#include <JuceHeader.h>

#include "SIDPlayer.h"

#include "libSidplayEZ/src/EZ/SidTuneInfoEZ.h"

#include "ultra-shared/Config/DataSource.h"

#include "Config/FilePaths.h"
#include "Config/HVSCSource.h"
#include "Config/TunePatches.h"

#include "SIDEffects.h"


//-----------------------------------------------------------------------------

namespace
{
	// Collection tunes reach the engine with their patches applied
	void loadCollectionBytes ( const char* fileName, std::vector<uint8_t>& bufferRef )
	{
		hvscsource::loadBytes ( fileName, bufferRef );
		tunepatches::apply ( fileName, bufferRef );
	}
}
//-----------------------------------------------------------------------------

SIDPlayer::SIDPlayer ()
	: juce::Thread ( "SIDPlayer" )
{
}
//-----------------------------------------------------------------------------

SIDPlayer::~SIDPlayer ()
{
	stopRender ();
}
//-----------------------------------------------------------------------------

bool SIDPlayer::load ( const char* filename )
{
	readyToPlay = false;
	paused = false;

	return engineEZ.loadSidFile ( filename );
}
//-----------------------------------------------------------------------------

bool SIDPlayer::load ( SidTune::LoaderFunc loader, const char* filename )
{
	readyToPlay = false;
	paused = false;

	return engineEZ.loadSidFile ( loader, filename );
}
//-----------------------------------------------------------------------------

bool SIDPlayer::loadTune ( const std::string& name )
{
	// "$HVSC$/..." keys: the engine gets the "/MUSICIANS/..." name the profile
	// selectors match on, the bytes come through the loader
	if ( name.starts_with ( filepaths::hvscMarker ) && name.size () > filepaths::hvscMarker.size () + 1 )
		return load ( loadCollectionBytes, name.c_str () + filepaths::hvscMarker.size () );

	if ( juce::File::isAbsolutePath ( name ) )
		return load ( name.c_str () );

	return load ( datasource::loadBytes, name.c_str () );
}
//-----------------------------------------------------------------------------

bool SIDPlayer::init ( const unsigned int songNo, const bool useFilter )
{
	// A real machine powers up with an arbitrary TOD clock; roll a fresh one per
	// (re)init so TOD-reading tunes vary between plays like on hardware
	engineEZ.setTodPowerOnSeed ( uint32_t ( juce::Random::getSystemRandom ().nextInt () ) | 1u );

	engineEZ.setTuneNumber ( songNo, useFilter );

	// setTuneNumber runs the tune's own init routine, which can hit an illegal opcode
	if ( engineEZ.isJammed () )
	{
		Z_ERR ( "Illegal instruction in the init routine of " << juce::String ( engineEZ.getFileInfo ().filename )
				<< " (song " << int ( songNo ) << ")" );

		readyToPlay = false;
		return false;
	}

	Z_DLOG ( "init with filter " << juce::String ( useFilter ? "on" : "off" ) );

	readyToPlay = false;
	paused = false;

	// Remove old buffers
	waveform.setSize ( 0, 0 );
	registers = {};

	rendered = false;
	renderStarted = false;
	faded = false;
	renderPlayOffset = 0;
	renderProgress = 0;

	// setTuneNumber applies the CSV profile, so the render thread's picture of
	// the engine is stale; force a full re-apply of any live tweaks
	lastAppliedProfile.reset ();
	if ( liveTweak )
		liveProfileDirty = true;
	ebuGainMain = 1.0f;
	ebuGain[ 0 ].setAndSnap ( 1.0f );
	ebuGain[ 1 ].setAndSnap ( 1.0f );

	readyToPlay = engineEZ.isReadyToPlay ();

	return readyToPlay;
}
//-----------------------------------------------------------------------------

bool SIDPlayer::play ( float* const* dst, int lengthWanted )
{
	if ( ! readyToPlay || paused )
		return false;

	// One snapshot for the whole callback: seek () can move the offset from the
	// message thread at any moment, and the length math and the read pointers
	// below must agree on the same value
	const auto	playOffset = renderPlayOffset.load ();

	const auto	toCopy = std::min ( {	renderProgress.load ( std::memory_order_acquire ) - playOffset,
										lengthWanted,
										waveform.getNumSamples () - playOffset } );
	lenLeft = toCopy;
	if ( toCopy <= 0 )
		return false;

	lengthWanted -= toCopy;

	auto copyGained = [ this, playOffset ] ( float* destination, const int channel, const int toCopy, const int restLength )
	{
		auto	source = waveform.getReadPointer ( channel, playOffset );

		if ( useReplayGain )
		{
			ebuGain[ channel ].set ( ebuGainMain );

			if ( ebuGain[ channel ].restingAtTarget () )
			{
				const auto	fixedGain = ebuGain[ channel ].get ();

				for ( auto i = 0; i < toCopy; ++i )
					destination[ i ] = source[ i ] * fixedGain;
			}
			else
			{
				for ( auto i = 0; i < toCopy; ++i )
					destination[ i ] = source[ i ] * ebuGain[ channel ].getAndStepSlow ();
			}
		}
		else
		{
			std::copy_n ( source, toCopy, destination );
		}

		std::fill_n ( destination + toCopy, restLength, 0.0f );
	};

	copyGained ( dst[ 0 ], 0, toCopy, lengthWanted );

	if ( waveform.getNumChannels () == 2 )
		copyGained ( dst[ 1 ], 1, toCopy, lengthWanted );

	// Advance only if no seek landed during the copy, the seek wins then
	auto	expected = playOffset;
	renderPlayOffset.compare_exchange_strong ( expected, playOffset + toCopy );

	return true;
}
//-----------------------------------------------------------------------------

bool SIDPlayer::startRender ( uint32_t _lengthMS, uint32_t _fadeOutLenMS, float _ebuLevel, uint32_t _skipMS )
{
	if ( ! readyToPlay )
		return false;

	renderLengthMs = _lengthMS;
	fadeOutLengthMS = _fadeOutLenMS;
	ebuLevel = _ebuLevel;
	skipStartMS = _skipMS;

	const juce::ScopedLock	sl ( waveformLock );

	return startThread ( juce::Thread::Priority::low );
}
//-----------------------------------------------------------------------------

void SIDPlayer::stopRender ()
{
	stopThread ( -1 );
}
//-----------------------------------------------------------------------------

void SIDPlayer::togglePlayPause ()
{
	if ( readyToPlay )
		paused = ! paused;
}
//-----------------------------------------------------------------------------

void SIDPlayer::seek ( uint32_t positionMS )
{
	renderPlayOffset = int ( positionMS * 44.1f );
}
//-----------------------------------------------------------------------------

bool SIDPlayer::finishedPlaying () const
{
	if ( ! readyToPlay || paused )
		return false;

	return rendered && lenLeft == 0 && renderPlayOffset;
}
//-----------------------------------------------------------------------------

int SIDPlayer::getDataIndex ( const int dataSize ) const
{
	const auto	realPlayOffset = std::max ( 0, renderPlayOffset - outputLatency - int ( sixtyHzLength ) );

	return std::min ( realPlayOffset / int ( sixtyHzLength ), dataSize - 1 );
}
//-----------------------------------------------------------------------------

std::pair<uint8_t*, int> SIDPlayer::getSidStatus ( int sidNum ) const
{
	jassert ( sidNum >= 0 && sidNum < getNumChips () );

	static uint8_t	dummy[ 32 ] = { 0 };

	// No song has been rendered yet, or a new one is loading and has fewer chips
	if ( sidNum < 0 || sidNum >= int ( registers.size () ) || registers[ sidNum ].empty () )
		return { &dummy[ 0 ], 1 };

	const auto	index = getDataIndex ( int ( registers[ sidNum ].size () ) );

	return { (uint8_t*)&registers[ sidNum ][ index ][ 0 ], index + 1 };
}
//-----------------------------------------------------------------------------

uint16_t SIDPlayer::getCPUCycles () const
{
	// `cycles` is reallocated on track change; tryEnter keeps the message
	// thread from blocking, a contended frame reads as 0
	if ( ! constructionLock.tryEnter () )
		return 0;

	uint16_t	ret = 0;

	// Zero until a song has been rendered
	if ( ! cycles.empty () )
		ret = cycles[ getDataIndex ( int ( cycles.size () ) ) ];

	constructionLock.exit ();

	return ret;
}
//-----------------------------------------------------------------------------

bool SIDPlayer::lockDigiBuffers ()
{
	return constructionLock.tryEnter ();
}
//-----------------------------------------------------------------------------

void SIDPlayer::unlockDigiBuffers ()
{
	constructionLock.exit ();
}
//-----------------------------------------------------------------------------

std::pair<int8_t*, int> SIDPlayer::getDigiBuffer ( int sidNum )
{
	jassert ( sidNum >= 0 && sidNum < getNumChips () );

	if ( sidNum < 0 || sidNum >= int ( digiWaveforms.size () ) || digiWaveforms[ sidNum ].size () < sixtyHzLength )
		return { nullptr, 0 };

	const auto	realPlayOffset = std::max ( 0, renderPlayOffset - outputLatency - int ( sixtyHzLength ) );
	const auto	offset = std::clamp ( realPlayOffset, 0, int ( digiWaveforms[ sidNum ].size () - sixtyHzLength ) );

	return { (int8_t*)digiWaveforms[ sidNum ].data () + offset, offset };
}
//-----------------------------------------------------------------------------

void SIDPlayer::run ()
{
	renderCore ( [ this ] { return threadShouldExit (); } );
}
//-----------------------------------------------------------------------------

void SIDPlayer::pushLiveProfile ( const ChipSettings& s )
{
	{
		const juce::SpinLock::ScopedLockType	sl ( liveProfileLock );

		liveProfile = s;
	}

	liveProfileDirty = true;
}
//-----------------------------------------------------------------------------

void SIDPlayer::applyLiveProfile ()
{
	if ( ! liveProfileDirty.exchange ( false ) )
		return;

	ChipSettings	s;
	{
		const juce::SpinLock::ScopedLockType	sl ( liveProfileLock );

		s = liveProfile;
	}

	// Only touch what changed: some setters rebuild large filter tables, and a
	// digi-volume nudge must not pay for a bandpass LUT rebuild
	const auto	last = lastAppliedProfile;

	if ( ! last || last->fltCapOld != s.fltCapOld )
		engineEZ.set6581Filter_uCoxAndCap ( 20.0, s.fltCapOld );
	if ( ! last || last->flt0Dac != s.flt0Dac )
		engineEZ.set6581FilterCurve ( s.flt0Dac );
	if ( ! last || last->fltGain != s.fltGain )
		engineEZ.set6581FilterGain ( s.fltGain );
	if ( ! last || last->fltSaturation != s.fltSaturation )
		engineEZ.set6581FilterSaturation ( s.fltSaturation );
	if ( ! last || last->fltResonance != s.fltResonance )
		engineEZ.set6581FilterResonance ( s.fltResonance );
	if ( ! last || last->waveDC != s.waveDC )
		engineEZ.set6581WaveDCOffset ( s.waveDC );
	if ( ! last || last->extInDC != s.extInDC )
		engineEZ.set6581ExtInDC ( s.extInDC );
	if ( ! last || last->voiceBias != s.voiceBias )
		engineEZ.set6581VoiceDCBias ( s.voiceBias );
	if ( ! last || last->leakageRate != s.leakageRate )
		engineEZ.set6581LeakageRate ( s.leakageRate );
	if ( ! last || last->cwsLevel != s.cwsLevel )
		engineEZ.setCombinedWaveforms ( reSIDfp::CombinedWaveforms ( s.cwsLevel ), 1.0f );
	if ( ! last || last->cwsSawPulseUltra != s.cwsSawPulseUltra )
		engineEZ.set6581SawPulseUltra ( s.cwsSawPulseUltra );

	lastAppliedProfile = s;
}
//-----------------------------------------------------------------------------

bool SIDPlayer::renderBlocking ( uint32_t lengthMS, uint32_t fadeOutLenMS, float _ebuLevel, uint32_t skipMS, const std::function<bool ()>& shouldAbort )
{
	if ( ! readyToPlay )
		return false;

	renderLengthMs = lengthMS;
	fadeOutLengthMS = fadeOutLenMS;
	ebuLevel = _ebuLevel;
	skipStartMS = skipMS;

	return renderCore ( shouldAbort );
}
//-----------------------------------------------------------------------------

juce::AudioBuffer<float> SIDPlayer::takeWaveform ()
{
	const juce::ScopedLock	sl ( waveformLock );

	return std::move ( waveform );
}
//-----------------------------------------------------------------------------

// The actual render loop, fills the waveform/register/digi buffers and
// measures loudness; returns false when aborted or the emulation stalls
bool SIDPlayer::renderCore ( const std::function<bool ()>& shouldAbort )
{
	juce::Thread::setCurrentThreadName ( engineEZ.getFileInfo ().filename );

	auto calcEbuGain = [ this ] ( const float level )
	{
		// The cap bounds the running-measurement transients (early integration
		// reads near-silence) and degenerate quiet tunes
		ebuGainMain = std::min ( 10.0f, juce::Decibels::decibelsToGain<float> ( targetLUFS - level ) );
	};

	const auto	measureLoudness = ebuLevel >= -0.1f || ebuLevel <= -95.9f;

	auto measureEbu = [ this, measureLoudness, &calcEbuGain ] ( const int offset, const int length )
	{
		if ( ! measureLoudness )
			return;

		// Measure loudness (only used for unknown songs)
		const float*	channels[ 2 ] = { waveform.getReadPointer ( 0 ) + offset,
										  waveform.getNumChannels () == 2 ? waveform.getReadPointer ( 1 ) + offset : nullptr };
		ebu.process ( channels, length );

		const auto	integrated = float ( ebu.effectiveLUFS () );

		calcEbuGain ( integrated );
	};

	if ( ! measureLoudness )
	{
		calcEbuGain ( ebuLevel );
		ebuGain[ 0 ].setAndSnap ( ebuGainMain );
		ebuGain[ 1 ].setAndSnap ( ebuGainMain );
		Z_DLOG ( "Gain: " + juce::String ( ebuGainMain ) );
	}
	else
	{
		Z_DLOG ( "Unknown song. Measuring gain..." );
	}

	auto	totalLength = uint32_t ( ( (uint64_t)renderLengthMs * 44100ull ) / 1000ull );	// convert to number of samples at 44.1kHz

	// Round up to next block-size
	totalLength = uint32_t ( ( ( (uint64_t)totalLength + sixtyHzLength - 1 ) / sixtyHzLength ) * sixtyHzLength );

	const auto	numSids = engineEZ.getNumChips ();
	const auto	isStereo = numSids >= 2;

	const auto	freshRender = ! renderStarted;

	// Allocate memory for output (skipped when resuming a paused render)
	if ( ! renderStarted )
	{
		const juce::ScopedLock	sl ( constructionLock );

		waveform.setSize ( isStereo ? 2 : 1, totalLength );
		ebu = PerceivedLoudness ( 44100.0, waveform.getNumChannels () );

		for ( auto& ssflt : subsonicFilter )
			ssflt.reset ();

		digiWaveforms.assign ( numSids, {} );
		for ( auto& digi : digiWaveforms )
			digi = std::vector<uninitialized<int8_t>> ( totalLength );

		const auto	regLength = totalLength / sixtyHzLength + 1;
		registers.assign ( numSids, {} );
		for ( auto& reg : registers )
			reg = regs ( regLength );

		cycles = std::vector<uninitialized<uint16_t>> ( totalLength / sixtyHzLength + 1 );

		renderStarted = true;
	}

	// A delayed start pre-renders the silent intro into scratch buffers: the
	// audible output begins where the music does. The songlength stays intact,
	// the HVSC doesn't count the silence either
	if ( freshRender && skipStartMS )
	{
		float	skipBuffer[ 2 ][ sixtyHzLength ];

		std::vector<int8_t>				skipDigi ( size_t ( numSids ) * sixtyHzLength );
		std::vector<std::span<int8_t>>	skipDigiPtrs ( numSids );

		for ( auto i = 0; i < numSids; ++i )
			skipDigiPtrs[ i ] = { skipDigi.data () + size_t ( i ) * sixtyHzLength, sixtyHzLength };

		auto	skipLeft = uint32_t ( uint64_t ( skipStartMS ) * 44100ull / 1000ull );

		while ( skipLeft )
		{
			if ( shouldAbort () )
				return false;

			const auto	requestLength = std::min ( skipLeft, sixtyHzLength );
			const auto	wordsWritten = engineEZ.runEmulation ( { skipBuffer[ 0 ], requestLength },
																isStereo ? std::span<float> { skipBuffer[ 1 ], requestLength } : std::span<float> {},
																skipDigiPtrs );

			if ( engineEZ.isJammed () )
			{
				Z_ERR ( "Illegal instruction while skipping the intro of " << juce::String ( engineEZ.getFileInfo ().filename )
						<< " (song " << int ( getCurrentSong () ) << ")" );
				return false;
			}

			skipLeft -= wordsWritten;
		}
	}

	// Render output into final buffers, continuing from renderProgress on resume
	{
		float	outBuffer[ 2 ][ sixtyHzLength ];

		// One scratch block per chip
		std::vector<int8_t>				digiBuffers ( size_t ( numSids ) * sixtyHzLength );
		std::vector<std::span<int8_t>>	digiPtrs ( numSids );

		for ( auto i = 0; i < numSids; ++i )
			digiPtrs[ i ] = { digiBuffers.data () + size_t ( i ) * sixtyHzLength, sixtyHzLength };

		auto	progress = renderProgress.load ();

		auto	lengthWanted = totalLength - uint32_t ( progress );
		auto	sidRegOffset = progress / int ( sixtyHzLength );

		auto	start = juce::Time::getHighResolutionTicks ();

		while ( lengthWanted > 0 )
		{
			if ( shouldAbort () )
				return false;

			// Live-tweak mode: apply pending profile changes, then hold the
			// render close to the playhead instead of racing ahead
			if ( liveTweak.load ( std::memory_order_relaxed ) )
			{
				applyLiveProfile ();

				while ( liveTweak && progress - renderPlayOffset.load () > liveAheadSamples )
				{
					if ( shouldAbort () )
						return false;

					juce::Thread::sleep ( 2 );
				}
			}

			const auto	requestLength = std::min ( lengthWanted, sixtyHzLength );		// generate only up to 60Hz worth of data
			const auto	wordsWritten = engineEZ.runEmulation ( { outBuffer[ 0 ], requestLength },
																isStereo ? std::span<float> { outBuffer[ 1 ], requestLength } : std::span<float> {},
																digiPtrs );

			// The tune's play routine runs here and can hit an illegal opcode
			if ( engineEZ.isJammed () )
			{
				Z_ERR ( "Illegal instruction while rendering " << juce::String ( engineEZ.getFileInfo ().filename )
						<< " (song " << int ( getCurrentSong () ) << ")" );
				return false;
			}

			if ( wordsWritten != requestLength )
				return false;

			// Apply subsonic filter
			for ( auto i = 0; i < waveform.getNumChannels (); ++i )
				subsonicFilter[ i ].process ( outBuffer[ i ], wordsWritten );

			// Write registers and digi buffers into their buffers
			for ( auto i = 0; i < numSids; ++i )
			{
				// Write SID registers into buffer
				engineEZ.getSidStatus ( i, (uint8_t*)&registers[ i ][ sidRegOffset ][ 0 ] );

				// Write digi samples into buffer
				std::memcpy ( digiWaveforms[ i ].data () + progress, digiPtrs[ i ].data (), wordsWritten );
			}

			// Write cycle count into buffer
			cycles[ sidRegOffset ] = engineEZ.getInterruptCycles ();

			++sidRegOffset;

			// Write audio into buffers
			std::copy_n ( outBuffer[ 0 ], wordsWritten, waveform.getWritePointer ( 0 ) + progress );
			if ( isStereo )
				std::copy_n ( outBuffer[ 1 ], wordsWritten, waveform.getWritePointer ( 1 ) + progress );

			//
			// Helper: Measure EBU loudness
			//
			measureEbu ( progress, wordsWritten );

			lengthWanted -= wordsWritten;
			progress += wordsWritten;

			// Publish AFTER the sample stores above, pairs with the acquire
			// load in play (), so visible progress means visible samples
			renderProgress.store ( progress, std::memory_order_release );
		}

		start = juce::Time::getHighResolutionTicks () - start;

		const auto	seconds = juce::Time::highResolutionTicksToSeconds ( start );
		const auto	wavSeconds = double ( waveform.getNumSamples () ) / 44100.0;

		Z_INFO ( "Time to render: " + juce::String ( seconds ) + " seconds - ratio: " + juce::String ( wavSeconds / seconds ) );
	}

	// Calculate LUFS for RAW output (no FX)
	if ( measureLoudness )
	{
		const auto	effective = float ( ebu.effectiveLUFS () );

		calcEbuGain ( effective );

		if ( effective > -96.0f )
		{
			auto	msg = "Measured " + juce::String ( ebu.integratedLUFS (), 1 ) + " LUFS, midband "
						+ juce::String ( ebu.midLUFS (), 1 ) + ", replay gain "
						+ juce::String ( std::min ( 20.0f, targetLUFS - effective ), 1 ) + " dB";

			const auto	punishment = effective - float ( ebu.integratedLUFS () );
			if ( punishment > 0.05f )
				msg << " (midband punishment " << juce::String ( punishment, 1 ) << " dB)";

			Z_INFO ( msg );
		}
		else
		{
			Z_INFO ( "No measurable loudness (silent tune)" );
		}

		if ( onLoudnessMeasured )
			onLoudnessMeasured ( float ( ebu.integratedLUFS () ), float ( ebu.midLUFS () ) );
	}

	if ( shouldAbort () )
		return false;

	//
	// Fade out (once, a resumed render must not fade again)
	//
	if ( fadeOutLengthMS && ! faded )
	{
		faded = true;

		const auto	fadeOutLength = ( fadeOutLengthMS * 44100u ) / 1000u;
		const auto	fadeOutStart = uint32_t ( waveform.getNumSamples () ) - fadeOutLength;

		for ( auto ch = 0; ch < waveform.getNumChannels (); ++ch )
		{
			auto	dst = waveform.getWritePointer ( ch ) + fadeOutStart;

			auto		startGain = 1.0f;
			const auto	decrement = 1.0f / float ( fadeOutLength );

			auto	numSamples = fadeOutLength;
			while ( numSamples-- )
			{
				*dst++ *= fast::pow2 ( startGain );
				startGain -= decrement;
			}
		}
	}

	if ( shouldAbort () )
		return false;

	const juce::ScopedLock	sl ( waveformLock );

	rendered = true;

	return true;
}
//-----------------------------------------------------------------------------
