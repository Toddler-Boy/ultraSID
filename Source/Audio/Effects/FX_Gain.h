#pragma once

#include <algorithm>

#include "FX_Helpers.h"

//-----------------------------------------------------------------------------

class FX_Gain final
{
public:
	void process ( float* __restrict__ srcDst, const int numSamples )
	{
		// Gains above 1.0 are legitimate here (bass-adjust compensation)
		if ( gain.restingAtTarget () )
		{
			const auto	g = gain.get ();

			if ( std::abs ( g - 1.0f ) < SmoothedValue::minVolume )
				return;

			if ( g < SmoothedValue::minVolume )
			{
				std::fill_n ( srcDst, numSamples, 0.0f );
				return;
			}

			for ( auto i = 0; i < numSamples; ++i )
				srcDst[ i ] *= g;

			return;
		}

		// Ramp gain
		for ( auto i = 0; i < numSamples; ++i )
			srcDst[ i ] *= gain.getAndStep ();
	}
	//-----------------------------------------------------------------------------

	void clearBuffers ()
	{
		gain.snap ();
	}
	//-----------------------------------------------------------------------------

	void setGain ( const float val )
	{
		gain.set ( val );
	}
	//-----------------------------------------------------------------------------

private:
	SmoothedValue	gain { 1.0f };
};
//-----------------------------------------------------------------------------
