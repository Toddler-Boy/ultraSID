#pragma once

#include <algorithm>

#include "Effects/FX_CheapTVSpeaker.h"
#include "Effects/FX_Delay.h"
#include "Effects/FX_Gain.h"
#include "Effects/FX_Noise.h"
#include "Effects/FX_Reverb.h"
#include "Effects/FX_Splitter.h"
#include "Effects/FX_TransformerHum.h"
#include "Effects/FX_WideMono.h"
#include "Effects/MultiBandEQ.h"
#include "LevelTracker.h"

//-----------------------------------------------------------------------------

// PURE, MAGIC and EPIC share ONE FX chain (splitter, widener, delay, reverb,
// EQ, noise). The modes live on a one-dimensional position axis (REAL 0 ..
// EPIC 3): every chain parameter derives from that position by interpolating
// between the neighboring modes' parameter sets, so a multi-hop switch
// audibly travels THROUGH the intermediate modes (REAL to EPIC first becomes
// PURE, then MAGIC), one transition_time per hop, and nothing ever clicks.
// PURE is "all FX at zero" with the crossover swept to a sub-audible parking
// frequency (a phase-coherent bypass; a dry/wet crossfade would comb against
// the splitter's allpass); once settled there the chain bypasses itself and
// PURE is bit-identical. REAL is a structurally different chain (hum,
// speaker, noise); the position's 0..1 segment crossfades the two chain
// outputs equal-power.

class SIDEffects final
{
public:
	SIDEffects ();

	enum FXMode : int8_t {
		REAL,
		PURE,
		MAGIC,
		EPIC,
		MYTHIC,

		numFXModes
	};

	// The FX parameter set: enum token and default value, one row per knob.
	// The defaults are what the export renderer runs on; playback overlays
	// the tweakable yml values until release settles them here.
	// transition_time = seconds per mode hop / 100 (0.03 = 3 s).
	// EPIC and MYTHIC mirror the MAGIC parameters they don't list
	#define FX_PARAMETERS(X) \
		X(stereo_processing,		1.0f) \
		X(transition_time,			0.03f) \
		/* REAL */ \
		X(real_distortion,			0.175f) \
		X(real_hum_volume,			0.07f) \
		X(real_noise_volume,		0.12f) \
		X(real_noise_color,			0.5f) \
		/* MAGIC */ \
		X(magic_splitter_freq,		0.2f) \
		X(magic_splitter_lowGain,	0.15f) \
		X(magic_wideMono_width,		0.5f) \
		X(magic_delay_feedback,		0.42f) \
		X(magic_delay_wet,			0.17f) \
		X(magic_reverb_wet,			0.32f) \
		X(magic_noise_volume,		0.05f) \
		X(magic_noise_color,		0.5f) \
		/* EPIC, the deliberately bolder trio */ \
		X(epic_wideMono_width,		0.8f) \
		X(epic_delay_feedback,		0.55f) \
		X(epic_delay_wet,			0.31f) \
		X(epic_reverb_wet,			0.45f) \
		/* MYTHIC, cranked to 11 */ \
		X(mythic_wideMono_width,	1.0f) \
		X(mythic_delay_feedback,	0.71f) \
		X(mythic_delay_wet,			0.46f) \
		X(mythic_reverb_wet,		0.6f)

	enum FXParameter : int8_t
	{
		#define X(param, def) param,
		FX_PARAMETERS ( X )
		#undef X

		numFXParameters
	};

	void setFXParameter ( const FXParameter idx, const float val );

	// User tone control (band 0..2 = low shelf, peak, high shelf): one GLOBAL
	// 3-band offset (-1..+1 per band) on top of the per-mode preset curves,
	// the user's adaptation to their listening environment. Presets live in
	// SIDEffects.cpp
	static constexpr float	userEqFreq[ 3 ] = { 200.0f, 1000.0f, 4000.0f };
	static constexpr float	userEqOffsetRange = 12.0f;	// dB at full deflection

	void setUserEQOffset ( const int band, const float offset );

	void setFXMode ( const int mode );
	void setVolume ( const float volume );
	void setChipModel ( const bool is6581, const bool isNTSC, const int width, const float bassAdjust );
	void setStereo ( const bool stereo ) { inputIsStereo = stereo; }

	// How much the two output channels differ: 0 = the same signal, 1 = decorrelated.
	// REAL and PURE keep a mono input mono (REAL's noise floor decorrelates the
	// channels only slightly), the FX chain widens it, so the value glides across the
	// PURE -> MAGIC segment with the transition. A stereo input is stereo in every mode
	[[ nodiscard ]] float outputStereoAmount () const
	{
		if ( ! isMono () )
			return 1.0f;

		return std::clamp ( easedPosition () - float ( FXMode::PURE ), 0.0f, 1.0f );
	}

	// The audible FX-mode position: settles exactly at REAL 0.0, PURE 1.0,
	// MAGIC 2.0, EPIC 3.0, MYTHIC 4.0 (the FXMode values) and travels through
	// the in-between during a transition (2.5 = halfway MAGIC to EPIC).
	// This IS the value driving the audio, smoothstepped so the journey eases
	// in and out. Poll from the UI update to drive the transition animation
	[[ nodiscard ]] float getTransitionPosition () const { return easedPosition (); }

	void process ( float* const* inOut, int numSamples );
	void applyGlain ( float* const* inOut, int numSamples );
	void clearBuffers ();
	void clearClipIndicators ();

	// While playback is stopped the render path keeps feeding zeros so the
	// tails ring out; drained() flips once the output stayed inaudible for a
	// second and processing may go idle
	void setPlaying ( const bool nowPlaying );
	[[ nodiscard ]] bool drained () const { return ! playing && tailSilentSamples >= drainWindowSamples; }

	// Completes any running mode transition instantly (also part of
	// clearBuffers): tune changes and measurement tools want the current mode
	// fully settled, not a leftover morph
	void snapFXTransition ();

	LevelTracker	inputLevel[ 2 ] = { { 240.0f }, { 240.0f } };
	LevelTracker	outputLevel[ 2 ] = { { 240.0f }, { 240.0f } };

private:
	static constexpr int	maxBlock = 1024;	// same cap as FX_Splitter

	// Linear ramp for the transition position: moves toward its target at a
	// fixed rate per block, retarget-safe mid-flight
	struct Ramp
	{
		float	value = 0.0f;
		float	target = 0.0f;

		void snap ()							{	value = target;	}
		void step ( const float maxDelta )		{	value = std::clamp ( target, value - maxDelta, value + maxDelta );	}
		[[ nodiscard ]] bool settled () const	{	return value == target;	}
	};

	[[ nodiscard ]] fxinline bool isMono () const { return ! inputIsStereo || downWidth < 1e-06f; };
	[[ nodiscard ]] bool fxChainSettledPure () const;

	// The whole journey from animFrom to the target eases in and out; the
	// eased position drives audio and animation alike
	[[ nodiscard ]] float easedPosition () const
	{
		const auto	span = position.target - animFrom;
		if ( span == 0.0f )
			return position.target;

		const auto	t = ( position.value - animFrom ) / span;
		return animFrom + span * t * t * ( 3.0f - 2.0f * t );
	}

	void applyPosition ( const float pos );
	void applyOutputGain ();
	void applyUserEQ ();
	void updateFXTargets ();
	void stepTransition ( const int numSamples );
	void processChunk ( float* const* inOut, const int numSamples );
	void processReal ( float* const* inOut, const int numSamples, const int numInputs );
	void processFXChain ( float* const* inOut, const int numSamples, const int numInputs );
	void clearFXChain ();
	void clearRealChain ();

	int		mode = FXMode::MAGIC;
	bool	inputIsStereo = true;
	float	downWidth = 0.2f;
	float	bassAdjust = 0.0f;
	bool	is6581 = false;

	bool	stereoProcessing = true;

	// The user's global 3-band offset, -1..+1 per band (see SIDEffects.cpp)
	float	userEqOffset[ 3 ] = {};

	//
	// REAL chain
	//
	FX_TransformerHum	realHum;			// Transformer hum to simulate shitty power supply
	FX_CheapTVSpeaker	realSpeaker;		// Terrible EQ + overdrive distortion to simulate a low-quality 2-inch speaker
	FX_Noise			realNoise;
	MultiBandEQ			realEQ;				// Carries only the user's listening-environment offset
	std::atomic<bool>	userEqActive = false;	// Flat offsets skip realEQ and (settled) the fx chain EQ

	//
	// The shared FX chain: PURE / MAGIC / EPIC morph its parameters
	//
	FX_Splitter			fxSplitter;
	FX_WideMono			fxWideMono;
	FX_Delay			fxDelay;
	FX_Reverb			fxReverb;
	MultiBandEQ			fxEQ;
	FX_Noise			fxNoise;

	// Mode-transition state: ONE master position in mode-space; everything
	// the chain morphs derives from it in applyPosition()
	Ramp	position;
	float	animFrom = 0.0f;		// where the current move started (smoothstep anchor)
	float	chainMixValue = 1.0f;	// derived: 0 = REAL chain, 1 = shared chain
	float	curTrimDb = -7.0f;		// derived: the mode output trim at the current position

	bool	fxChainBypassed = false;
	int		eqSettleSamples = 1 << 30;	// samples since the fxEQ gains last moved

	// Tail drain: below the threshold counts as silence, a full window of it
	// means the chain is drained (starts drained, nothing played yet)
	static constexpr float	drainSilenceThreshold = 1.0e-4f;	// -80 dBFS
	static constexpr int	drainWindowSamples = 44100;

	bool	playing = false;
	int		tailSilentSamples = drainWindowSamples;

	float	realScratch[ 2 ][ maxBlock ];	// REAL renders here during the crossfade

	// chainMixValue only moves once per block; the REAL gain is the complement
	SmoothedValue	chainFade { 1.0f };

	// Output gain
	float		volume = 1.0f;
	FX_Gain		gain[ 2 ];

	// Parameters for all FX, index-matched to FXParameter by the shared list
	float	fxParams[ numFXParameters ] =
	{
		#define X(param, def) def,
		FX_PARAMETERS ( X )
		#undef X
	};
};
//-----------------------------------------------------------------------------
