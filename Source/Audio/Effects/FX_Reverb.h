#pragma once

#include "FX_Helpers.h"

#if defined (__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wshadow"
	#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#endif

#include "MVerb.h"

#if defined (__clang__)
	#pragma clang diagnostic pop
#endif
//-----------------------------------------------------------------------------

class FX_Reverb final
{
public:
	FX_Reverb ()
	{
		mverb.setParameter ( MVerb<float>::DAMPINGFREQ, 0.7f );
		mverb.setParameter ( MVerb<float>::DENSITY, 1.0f );
		mverb.setParameter ( MVerb<float>::BANDWIDTHFREQ, 0.9f );
		mverb.setParameter ( MVerb<float>::DECAY, 0.5f );
		mverb.setParameter ( MVerb<float>::PREDELAY, 0.1f );
		mverb.setParameter ( MVerb<float>::SIZE, 1.0f );
		mverb.setParameter ( MVerb<float>::EARLYMIX, 0.75f );
		applyWet ( 0.125f );
	}
	//--------------------------------------------------------------------------------

	void process ( float* const* __restrict__ srcDst, const int numSamples )
	{
		mverb.process ( const_cast<float**>( srcDst ), const_cast<float**>( srcDst ), numSamples );
	}
	//--------------------------------------------------------------------------------

	void clearBuffers ()
	{
		mverb.reset ();
	}
	//--------------------------------------------------------------------------------

	void setWet ( const float mix )
	{
		applyWet ( fast::pow2 ( mix ) );
	}
	//--------------------------------------------------------------------------------

private:
	// Send-style topology, like a console send/return: reverb is ADDED on top
	// (out = dry + w * wet), the dry signal never attenuates. A dry/wet crossfade
	// would replace direct signal with diffuse energy, lowering the
	// direct-to-reverberant ratio, which the ear reads as distance, and step the
	// whole processed band into the background. The band-level cost of adding
	// energy is handled by FX_Splitter::setLowGain instead. MIX = w/(1+w) and
	// GAIN = 1+w through MVerb's stock crossfade yields exactly dry + w * wet
	// (GAIN is unsmoothed in MVerb, but per-slider-tick steps are < 0.1 dB, inaudible)
	void applyWet ( const float w )
	{
		mverb.setParameter ( MVerb<float>::MIX, w / ( 1.0f + w ) );
		mverb.setParameter ( MVerb<float>::GAIN, 1.0f + w );
	}

	MVerb<float>	mverb;
};
//-----------------------------------------------------------------------------
