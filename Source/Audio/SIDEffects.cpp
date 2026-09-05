#include <algorithm>
#include <cmath>
#include <iterator>

#include "SIDEffects.h"

#include "libSidplayEZ/src/EZ/dsp-downmix.h"

#include "Effects/FX_Helpers.h"

//-----------------------------------------------------------------------------

using enum SIDEffects::FXParameter;

// Everything that varies from mode to mode, one row per FXMode. The dry modes borrow
// MAGIC's parameter slots where a number must exist, fxOn gates them to zero anyway
struct ModeSettings
{
	SIDEffects::FXParameter	width, delayWet, delayFeedback, reverbWet;

	float	extraLowGain;	// on top of the shared splitter low gain, dry mono band only
	float	trimDb;			// output trim, tuned by ear
	float	eqDb[ 3 ];		// EQ preset per band, the user's global offset sits on top
};

constexpr ModeSettings	modeSettings[] =
{
	// REAL, speaker EQ + distortion drive; middy and hissy, reads louder than it measures
	{ magic_wideMono_width,  magic_delay_wet,  magic_delay_feedback,  magic_reverb_wet,  0.0f, -8.5f, { 0.0f, 0.0f, 0.0f } },

	// PURE, only the user EQ adds energy
	{ magic_wideMono_width,  magic_delay_wet,  magic_delay_feedback,  magic_reverb_wet,  0.0f, -5.0f, { 0.0f, 0.0f, 0.0f } },

	// MAGIC, send-style FX + low-band trim + user EQ
	{ magic_wideMono_width,  magic_delay_wet,  magic_delay_feedback,  magic_reverb_wet,  0.0f, -7.0f, { 4.0f, 2.0f, 3.0f } },

	// EPIC, the same chain with hotter settings and bolder bass
	{ epic_wideMono_width,   epic_delay_wet,   epic_delay_feedback,   epic_reverb_wet,   1.5f, -7.0f, { 4.0f, 2.0f, 3.0f } },

	// MYTHIC, cranked; its extra character comes from the FX, not the EQ
	{ mythic_wideMono_width, mythic_delay_wet, mythic_delay_feedback, mythic_reverb_wet, 3.0f, -8.0f, { 4.0f, 2.0f, 3.0f } },
};
static_assert ( std::size ( modeSettings ) == SIDEffects::numFXModes, "one settings row per mode" );
static_assert ( std::size ( modeSettings[ 0 ].eqDb ) == std::size ( SIDEffects::userEqFreq ), "one gain per EQ band" );

// USER EQ edits react at this speed (the chain EQ's internal smoothing), fast
// so the settings curve feels immediate; the mode PRESET part of the EQ morphs
// with the transition position instead
constexpr double	userEqSmoothingMs = 25.0;

// Settle tail before the bypassed PURE chain may skip its EQ
constexpr int	eqSettleThresholdSamples = 44100 / 2;

// PURE parks the crossover below the audible band instead of crossfading the
// chain against a dry path: a dry/processed blend combs audibly (the split is
// an allpass, the two paths are phase-rotated against each other), a swept
// crossover stays phase-coherent all the way and hands over silently
constexpr float	splitterParkHz = 10.0f;

//-----------------------------------------------------------------------------

SIDEffects::SIDEffects ()
{
	// Both EQs get their bands configured once; from here on only gains change
	{
		static constexpr MultiBandEQ::type	bandTypes[ 3 ] = { MultiBandEQ::lowShelf, MultiBandEQ::peak, MultiBandEQ::highShelf };

		for ( auto band = 0; band < 3; ++band )
		{
			realEQ.setBand ( band, bandTypes[ band ], userEqFreq[ band ], 0.0 );
			fxEQ.setBand ( band, bandTypes[ band ], userEqFreq[ band ], 0.0 );
		}

		fxEQ.setSmoothingTime ( userEqSmoothingMs );
	}

	for ( auto i = 0; const auto par : fxParams )
		setFXParameter ( FXParameter ( i++ ), par );

	snapFXTransition ();
}
//-----------------------------------------------------------------------------

void SIDEffects::setFXParameter ( const FXParameter idx, const float val )
{
	fxParams[ idx ] = val;

	// Stereo processing
	{
		stereoProcessing = fxParams[ FXParameter::stereo_processing ] > 0.5f;
	}

	// REAL
	{
		realSpeaker.setDistortion ( fxParams[ FXParameter::real_distortion ] * 10.0f );

		realHum.setVolume ( fxParams[ FXParameter::real_hum_volume ] );

		realNoise.setVolume ( fxParams[ FXParameter::real_noise_volume ] );
		realNoise.setColor ( fxParams[ FXParameter::real_noise_color ] );
	}

	// Shared chain, position-independent parameters
	{
		fxNoise.setColor ( fxParams[ FXParameter::magic_noise_color ] );
	}

	// Everything else derives from the transition position
	updateFXTargets ();
}
//-----------------------------------------------------------------------------

void SIDEffects::setFXMode ( const int _mode )
{
	mode = std::clamp ( _mode, int ( FXMode::REAL ), int ( FXMode::MYTHIC ) );

	updateFXTargets ();
}
//-----------------------------------------------------------------------------

void SIDEffects::setVolume ( const float _volume )
{
	volume = _volume;

	applyOutputGain ();
	clearClipIndicators ();
}
//-----------------------------------------------------------------------------

void SIDEffects::setChipModel ( const bool _is6581, const bool isNTSC, const int width, const float _bassAdjust )
{
	bassAdjust = _bassAdjust;
	is6581 = _is6581;

	//
	// REAL
	//
	{
		realHum.setFrequency ( isNTSC ? 60.0f : 50.0f );
	}

	//
	// Shared chain
	//
	{
		// Set stereo width (only relevant for 2SID and 3SID)
		downWidth = width * 0.01f;
	}

	updateFXTargets ();
}
//-----------------------------------------------------------------------------

void SIDEffects::setUserEQOffset ( const int band, const float offset )
{
	if ( band < 0 || band > 2 )
		return;

	userEqOffset[ band ] = std::clamp ( offset, -1.0f, 1.0f );

	applyUserEQ ();
	applyPosition ( easedPosition () );
}
//-----------------------------------------------------------------------------

// The user-offset half of the EQ story: realEQ carries only the offsets, and
// the flat/active flag gates both EQs for the bit-identical modes. The chain
// EQ's gains (preset + offset) are applied by applyPosition()
void SIDEffects::applyUserEQ ()
{
	auto	flat = true;
	for ( auto band = 0; band < 3; ++band )
	{
		const auto	userDb = userEqOffset[ band ] * userEqOffsetRange;

		realEQ.setGain ( band, userDb );

		flat = flat && std::fabs ( userDb ) < 0.001f;
	}

	// Flat offsets skip realEQ (and, once settled, the bypassed chain's EQ),
	// so untouched REAL and PURE stay bit-identical
	userEqActive = ! flat;
	eqSettleSamples = 0;
}
//-----------------------------------------------------------------------------

// Any configuration change funnels through here: retarget the position if the
// mode moved, then re-derive the chain from the current (possibly mid-flight)
// position
void SIDEffects::updateFXTargets ()
{
	if ( position.target != float ( mode ) )
	{
		// Anchor on the eased value, which is what the chain is actually set to;
		// the raw ramp position runs ahead of it and would step the whole chain
		animFrom = easedPosition ();
		position.value = animFrom;
		position.target = float ( mode );
	}

	applyUserEQ ();
	applyPosition ( easedPosition () );
	clearClipIndicators ();
}
//-----------------------------------------------------------------------------

// Everything the shared chain morphs, evaluated at the two neighboring
// integer mode positions and interpolated. The REAL segment (0..1) keeps the
// chain at PURE settings while chainMix crossfades the outputs
void SIDEffects::applyPosition ( const float pos )
{
	struct ChainState
	{
		float	width, delayWet, delayFeedback, reverbWet;
		float	lowGainDb, noiseVolume, splitFreqLog2, trimDb;
		float	eqDb[ 3 ];
	};

	const auto	stateFor = [ this ] ( const int m ) -> ChainState
	{
		ChainState	s;

		const auto	fxOn = m >= FXMode::MAGIC;
		const auto&	ms = modeSettings[ m ];

		s.width = fxOn ? fxParams[ ms.width ] : 0.0f;
		s.delayWet = fxOn ? fxParams[ ms.delayWet ] : 0.0f;
		s.reverbWet = fxOn ? fxParams[ ms.reverbWet ] : 0.0f;
		s.noiseVolume = fxOn ? fxParams[ FXParameter::magic_noise_volume ] : 0.0f;

		// feedback stays meaningful even where wet is 0
		s.delayFeedback = fxParams[ ms.delayFeedback ];

		s.lowGainDb = fxOn ? fxParams[ FXParameter::magic_splitter_lowGain ] * 10.0f + ms.extraLowGain : 0.0f;
		s.splitFreqLog2 = std::log2 ( fxOn ? fxParams[ FXParameter::magic_splitter_freq ] * 1000.0f : splitterParkHz );

		const auto	gainCompensation = bassAdjust * ( bassAdjust <= 0.0f ? 0.9f : 0.5f );
		s.trimDb = ms.trimDb - ( fxOn ? gainCompensation : 0.0f );

		// The 6581 has less bass, 2 dB more compensates; the
		// automatic bass offsets only apply to the FX-heavy modes
		const auto	autoLow = bassAdjust + is6581 * 2.0f;
		for ( auto band = 0; band < 3; ++band )
			s.eqDb[ band ] = ms.eqDb[ band ] + ( band == 0 && fxOn ? autoLow : 0.0f );

		return s;
	};

	// The last segment starts one below the top mode, since stateFor ( seg + 1 ) reads it
	const auto	seg = std::clamp ( int ( pos ), 0, numFXModes - 2 );
	const auto	f = pos - float ( seg );
	const auto	a = stateFor ( seg );
	const auto	b = stateFor ( seg + 1 );

	const auto	mix = [ f ] ( const float x, const float y ) { return fast::lerp ( x, y, f ); };

	fxWideMono.setWidth ( mix ( a.width, b.width ) );
	fxDelay.setWet ( mix ( a.delayWet, b.delayWet ) );
	fxDelay.setFeedback ( mix ( a.delayFeedback, b.delayFeedback ) );
	fxReverb.setWet ( mix ( a.reverbWet, b.reverbWet ) );
	fxSplitter.setLowGain ( mix ( a.lowGainDb, b.lowGainDb ) );
	fxSplitter.setFrequency ( std::exp2 ( mix ( a.splitFreqLog2, b.splitFreqLog2 ) ) );
	fxNoise.setVolume ( mix ( a.noiseVolume, b.noiseVolume ) );

	for ( auto band = 0; band < 3; ++band )
		fxEQ.setGain ( band, mix ( a.eqDb[ band ], b.eqDb[ band ] ) + userEqOffset[ band ] * userEqOffsetRange );

	curTrimDb = mix ( a.trimDb, b.trimDb );
	applyOutputGain ();

	chainMixValue = std::clamp ( pos, 0.0f, 1.0f );
}
//-----------------------------------------------------------------------------

// Advances the transition position by one block and re-derives the chain
void SIDEffects::stepTransition ( const int numSamples )
{
	const auto	wasBypassed = fxChainBypassed;

	if ( ! position.settled () )
	{
		// User-adjustable pace, seconds per mode hop (the position moves at
		// constant mode-space speed, so REAL to EPIC takes three hops)
		const auto	hopSeconds = std::clamp ( fxParams[ FXParameter::transition_time ] * 100.0f, 0.1f, 10.0f );
		const auto	prev = position.value;

		// The shared chain is about to become audible again: clean buffers,
		// its content would be a stale tail from the last time it ran
		if ( prev == 0.0f && position.target > 0.0f )
			clearFXChain ();

		position.step ( float ( numSamples ) / ( 44100.0f * hopSeconds ) );

		// REAL becomes audible the moment the position drops below PURE
		if ( prev >= 1.0f && position.value < 1.0f )
			clearRealChain ();

		applyPosition ( easedPosition () );
		eqSettleSamples = 0;
	}
	else
		eqSettleSamples = std::min ( eqSettleSamples + numSamples, 1 << 30 );

	// Leaving the settled-PURE bypass: the frozen tails are stale by definition
	const auto	bypassed = fxChainSettledPure ();
	if ( wasBypassed && ! bypassed )
		clearFXChain ();
	fxChainBypassed = bypassed;
}
//-----------------------------------------------------------------------------

bool SIDEffects::fxChainSettledPure () const
{
	return mode == FXMode::PURE && position.settled ();
}
//-----------------------------------------------------------------------------

void SIDEffects::process ( float* const* inOut, int numSamples )
{
	// The chain works out of fixed-size scratch buffers; the device block can be larger
	float*	chunkIO[ 2 ] = { inOut[ 0 ], inOut[ 1 ] };

	while ( numSamples > 0 )
	{
		const auto	todo = std::min ( numSamples, maxBlock );

		processChunk ( chunkIO, todo );

		chunkIO[ 0 ] += todo;
		chunkIO[ 1 ] += todo;
		numSamples -= todo;
	}
}
//-----------------------------------------------------------------------------

void SIDEffects::processChunk ( float* const* inOut, const int numSamples )
{
	const auto	numInputs = isMono () ? 1 : 2;

	// Use down mixer, if not mono, and stereo-processing is enabled
	if ( inputIsStereo )
	{
		auto	newWidth = 1.0f;

		if ( isMono () )
			newWidth = 0.0f;
		else if ( stereoProcessing )
			newWidth = downWidth;

		libsidplayEZ::dsp::downMix ( inOut[ 0 ], inOut[ 1 ], numSamples, newWidth );
	}

	for ( auto i = 0; i < numInputs; ++i )
		inputLevel[ i ].trackBuffer ( inOut[ i ], numSamples );

	stepTransition ( numSamples );

	//
	// Apply FX. chainMix crossfades between the REAL chain and the shared
	// chain; away from a REAL transition exactly one of them runs
	//
	const auto	mix = chainMixValue;

	if ( mix == 0.0f )
	{
		// Held at the endpoint while bypassed, so the next transition starts from it
		chainFade.setAndSnap ( 0.0f );

		processReal ( inOut, numSamples, numInputs );
		return;
	}

	if ( mix < 1.0f )
	{
		// Both chains are audible: REAL renders on a scratch copy of the
		// input, then the outputs crossfade equal-power
		std::copy_n ( inOut[ 0 ], numSamples, realScratch[ 0 ] );
		std::copy_n ( inOut[ numInputs > 1 ? 1 : 0 ], numSamples, realScratch[ 1 ] );

		float*	realIO[ 2 ] = { realScratch[ 0 ], realScratch[ 1 ] };
		processReal ( realIO, numSamples, numInputs );
		processFXChain ( inOut, numSamples, numInputs );

		// Linear, not equal-power: both chains carry the same tune, so they add
		// coherently and gains summing to 1 hold the level
		chainFade.set ( mix );

		for ( auto i = 0; i < numSamples; ++i )
		{
			const auto	chainGain = chainFade.getAndStep ();
			const auto	realGain = 1.0f - chainGain;

			inOut[ 0 ][ i ] = inOut[ 0 ][ i ] * chainGain + realScratch[ 0 ][ i ] * realGain;
			inOut[ 1 ][ i ] = inOut[ 1 ][ i ] * chainGain + realScratch[ 1 ][ i ] * realGain;
		}
		return;
	}

	chainFade.setAndSnap ( 1.0f );

	processFXChain ( inOut, numSamples, numInputs );
}
//-----------------------------------------------------------------------------

void SIDEffects::processReal ( float* const* inOut, const int numSamples, const int numInputs )
{
	realHum.process ( inOut, numSamples, numInputs );

	// Force stereo
	if ( numInputs == 1 )
		std::copy_n ( inOut[ 0 ], numSamples, inOut[ 1 ] );

	realSpeaker.process ( inOut, numSamples );

	if ( userEqActive )
		realEQ.process ( inOut[ 0 ], inOut[ 1 ], numSamples );

	realNoise.process ( inOut, numSamples );
}
//-----------------------------------------------------------------------------

void SIDEffects::processFXChain ( float* const* inOut, const int numSamples, const int numInputs )
{
	// Settled PURE: the chain bypasses itself entirely, an untouched PURE
	// stays bit-identical (only the user's EQ offsets may still run). The
	// crossover swept to its sub-audible parking frequency on the way here,
	// so this handoff is phase-coherent
	if ( fxChainBypassed )
	{
		// Create stereo signal
		if ( ! inputIsStereo )
			std::copy_n ( inOut[ 0 ], numSamples, inOut[ 1 ] );

		if ( userEqActive || eqSettleSamples < eqSettleThresholdSamples )
			fxEQ.process ( inOut[ 0 ], inOut[ 1 ], numSamples );

		return;
	}

	fxSplitter.splitBands ( inOut, numSamples, numInputs );

	// wideMono converts to stereo
	if ( isMono () )
		fxWideMono.process ( inOut, numSamples );

	// stereo
	fxDelay.process ( inOut, numSamples );
	fxReverb.process ( inOut, numSamples );

	fxSplitter.mergeBands ( inOut, numSamples );

	fxEQ.process ( inOut[ 0 ], inOut[ 1 ], numSamples );
	fxNoise.process ( inOut, numSamples );
}
//-----------------------------------------------------------------------------

void SIDEffects::applyGlain ( float* const* inOut, int numSamples )
{
	// Apply gain
	for ( auto channel = 0; channel < 2; ++channel )
	{
		gain[ channel ].process ( inOut[ channel ], numSamples );
		outputLevel[ channel ].trackBuffer ( inOut[ channel ], numSamples );
	}

	// While stopped, watch the post-gain output (what the listener hears)
	// until the tail has stayed inaudible for a full window
	if ( ! playing && tailSilentSamples < drainWindowSamples )
	{
		auto	peak = 0.0f;
		for ( auto channel = 0; channel < 2; ++channel )
			for ( auto i = 0; i < numSamples; ++i )
				peak = std::max ( peak, std::abs ( inOut[ channel ][ i ] ) );

		tailSilentSamples = peak < drainSilenceThreshold ? tailSilentSamples + numSamples : 0;
	}
}
//-----------------------------------------------------------------------------

void SIDEffects::setPlaying ( const bool nowPlaying )
{
	if ( playing == nowPlaying )
		return;

	playing = nowPlaying;
	tailSilentSamples = 0;

	realHum.setActive ( nowPlaying );
	realNoise.setActive ( nowPlaying );
	fxNoise.setActive ( nowPlaying );
}
//-----------------------------------------------------------------------------

void SIDEffects::clearFXChain ()
{
	fxSplitter.reset ();
	fxWideMono.clearBuffers ();
	fxDelay.clearBuffers ();
	fxReverb.clearBuffers ();
	fxEQ.reset ();
}
//-----------------------------------------------------------------------------

void SIDEffects::clearRealChain ()
{
	realSpeaker.clearBuffers ();
	realEQ.reset ();
}
//-----------------------------------------------------------------------------

void SIDEffects::snapFXTransition ()
{
	position.snap ();
	animFrom = position.value;

	applyPosition ( position.value );

	// The chain EQ's own gain smoothing snaps too
	for ( auto band = 0; band < 3; ++band )
		fxEQ.setGainImmediate ( band, fxEQ.getGain ( band ) );

	eqSettleSamples = 1 << 30;
	fxChainBypassed = fxChainSettledPure ();
}
//-----------------------------------------------------------------------------

void SIDEffects::clearBuffers ()
{
	clearClipIndicators ();

	clearRealChain ();
	clearFXChain ();

	// A tune change also ends any running mode transition
	snapFXTransition ();
}
//-----------------------------------------------------------------------------

void SIDEffects::clearClipIndicators ()
{
	inputLevel[ 0 ].clearClip ();
	inputLevel[ 1 ].clearClip ();
	outputLevel[ 0 ].clearClip ();
	outputLevel[ 1 ].clearClip ();
}
//-----------------------------------------------------------------------------

void SIDEffects::applyOutputGain ()
{
	const auto	outGain = fast::pow2 ( volume ) * fx::helpers::dbToLin ( curTrimDb );

	for ( auto& gn : gain )
		gn.setGain ( outGain );
}
//-----------------------------------------------------------------------------
