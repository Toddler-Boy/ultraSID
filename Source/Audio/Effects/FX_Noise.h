#pragma once

#include "FX_Helpers.h"

//-----------------------------------------------------------------------------

class FX_Noise final
{
public:
	void process ( float* const* __restrict__ srcDst, const int numSamples )
	{
		if ( gate.restingAtZero () )
			return;

		auto coloredNoise = [ this ] ( const int ch ) -> float
		{
			seed = ( 1103515245 * seed + 12345 );

			const auto	normalized = (float)( seed & 0x7FFFFFFF ) / 2147483647.0f;
			const auto	whitenoise = normalized - 0.5f;

			curNoise[ ch ] += color * ( whitenoise - curNoise[ ch ] );
			return curNoise[ ch ];
		};

		// Both channels walk the same gate ramp
		auto	stepped = gate;

		for ( auto ch = 0; ch < 2; ++ch )
		{
			stepped = gate;

			for ( auto i = 0; i < numSamples; ++i )
				srcDst[ ch ][ i ] += coloredNoise ( ch ) * volume * stepped.getAndStep ();
		}

		gate = stepped;
	}
	//-----------------------------------------------------------------------------

	// A generator keeps adding signal forever, so an idle chain fades it out
	// to let the output drain to true silence
	void setActive ( const bool active )
	{
		gate.set ( active ? 1.0f : 0.0f );
	}
	//-----------------------------------------------------------------------------

	void setVolume ( const float vol )
	{
		volume = fast::pow2 ( vol );
	}
	//-----------------------------------------------------------------------------

	void setColor ( const float col )
	{
		color = fast::pow2 ( col );
	}
	//-----------------------------------------------------------------------------

private:
	unsigned int	seed = 12345;
	float			volume = 0.0025f;
	float			color = 0.5f;
	float			curNoise[ 2 ] = {};
	SmoothedValue	gate { 1.0f };
};
//-----------------------------------------------------------------------------
