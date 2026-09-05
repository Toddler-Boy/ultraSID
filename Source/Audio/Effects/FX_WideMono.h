#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>

//
//	WIDE MONO
//
//	Pseudo stereo widening for mono sound that keeps the sound mono-compatible!
//

//-----------------------------------------------------------------------------

class FX_WideMono final
{
public:
	FX_WideMono ()
	{
		setWidth ( 0.5f );
	}
	//--------------------------------------------------------------------------------

	void process ( float* const* __restrict__ srcDst, int numSamples )
	{
		for ( auto i = 0; i < numSamples; ++i )
		{
			const auto	mid = srcDst[ 0 ][ i ];

			// create side information
			const auto	side = delay[ readWriteOffset ] * sideWidth;
			delay[ readWriteOffset ] = mid;

			// mix mid with new side; the mid passes at unity to keep the phantom
			// center at the forefront, the side adds its energy on top (~+1 dB at
			// width 0.5, accounted for by FX_Splitter::setLowGain). No makeup boost
			// (it biases the band balance) and no power compensation (it hollows
			// out the center)
			srcDst[ 0 ][ i ] = mid + side;
			srcDst[ 1 ][ i ] = mid - side;

			// update read/write offset
			++readWriteOffset;
			readWriteOffset %= delayTimeInSamples;
		}
	}
	//--------------------------------------------------------------------------------

	void clearBuffers ()
	{
		readWriteOffset = 0;
		std::ranges::fill ( delay, 0.0f );
	}
	//--------------------------------------------------------------------------------

	void setWidth ( const float width )
	{
		sideWidth = width;
	}
	//--------------------------------------------------------------------------------

private:
	float	sideWidth = 0.0f;

	static constexpr auto	delayTimeInSamples = 512u;	// Comes to around 11.6 ms at 44.1 kHz
	static_assert( ( delayTimeInSamples > 1 ) & ! ( delayTimeInSamples & ( delayTimeInSamples - 1 ) ), "Delay-time must be a power of two." );

	unsigned int	readWriteOffset = 0;
	float			delay[ delayTimeInSamples ] = {};
};
//-----------------------------------------------------------------------------
