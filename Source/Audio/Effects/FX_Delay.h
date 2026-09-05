#pragma once

#include <algorithm>
#include <iterator>

#include "FX_Helpers.h"

//-----------------------------------------------------------------------------

class FX_Delay final
{
public:
	void process ( float* const* __restrict__ srcDst, const int numSamples )
	{
		// send-style: echoes are added on top, the dry passes untouched
		// (no dry/wet crossfade, see FX_Reverb.h for why)
		for ( auto i = 0; i < numSamples; ++i )
		{
			const auto	outL = buffer[ 0 ][ bufferIndex ];
			const auto	outR = buffer[ 1 ][ bufferIndex ];

			buffer[ 0 ][ bufferIndex ] = srcDst[ 0 ][ i ] + feedback * outR;
			buffer[ 1 ][ bufferIndex ] = srcDst[ 1 ][ i ] + feedback * outL;

			srcDst[ 0 ][ i ] += outR * wet;
			srcDst[ 1 ][ i ] += outL * wet;

			++bufferIndex;
			bufferIndex %= delayTimeInSamples;
		}
	}
	//-----------------------------------------------------------------------------

	void clearBuffers ()
	{
		bufferIndex = 0;
		std::ranges::fill ( buffer[ 0 ], 0.0f );
		std::ranges::fill ( buffer[ 1 ], 0.0f );
	}
	//-----------------------------------------------------------------------------

	void setFeedback ( const float val )
	{
		feedback = fast::pow2 ( val );
	}
	//-----------------------------------------------------------------------------

	void setWet ( const float val )
	{
		wet = fast::pow2 ( val );
	}
	//-----------------------------------------------------------------------------

private:
	float	feedback = 0.1f;
	float	wet = 0.03f;

	static constexpr auto	delayTimeInSamples = 16384u * 2;	// Comes to around 371.5 ms at 44.1 kHz
	static_assert( ( delayTimeInSamples > 1 ) & ! ( delayTimeInSamples& ( delayTimeInSamples - 1 ) ), "Delay-time must be a power of two." );

	unsigned int	bufferIndex = 0;
	float			buffer[ 2 ][ delayTimeInSamples ] = {};
};
//-----------------------------------------------------------------------------
