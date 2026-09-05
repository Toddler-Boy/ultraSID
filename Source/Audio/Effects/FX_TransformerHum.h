#pragma once

#include <cmath>
#include <numbers>

#include "FX_Helpers.h"

//-----------------------------------------------------------------------------
//
// The hum of a cheap early-80s mains transformer LEAKING into the audio path
// (the electrical leak, not the acoustic sound of the core itself): an
// additive cosine series over ALL multiples of the mains frequency. Three
// shape ingredients, ear-tuned and frozen (only the volume stays a parameter):
//   fundamental: induction / ground-loop pickup of the mains itself
//   odd:         the undersized core saturates and flat-tops its magnetizing
//                current, the odd-harmonic grit of cheap iron
//   tilt:        the rectifier's narrow diode-conduction spikes couple as
//                di/dt, tilting energy up the series into a buzz
// The harmonic table is normalized to RMS 1, so the shape only changes the
// character, never the level. cos ( k * theta ) comes from a Chebyshev
// recurrence: one real cosine per sample. Two amplitude-modulation layers, a
// sub-Hz breath and a few-Hz flutter, keep the loop from reading as a frozen
// sample; old iron breathes under load and rattles its laminations.
//

class FX_TransformerHum final
{
public:
	FX_TransformerHum ()
	{
		buildHarmonics ();
	}
	//-----------------------------------------------------------------------------

	void process ( float* const* __restrict__ srcDst, const int numSamples, const int numChannels )
	{
		if ( gate.restingAtZero () )
			return;

		for ( auto i = 0; i < numSamples; ++i )
		{
			phase += phaseInc;
			phase -= int ( phase );

			// The recurrence amplifies seed error by up to k^2, so the one master
			// cosine is the real thing in double, not a polynomial approximation
			const auto	c = std::cos ( phase * 2.0 * std::numbers::pi );

			auto	km2 = 1.0;
			auto	km1 = c;
			auto	sum = amp[ 0 ] * c;

			for ( auto k = 1; k < numHarmonics; ++k )
			{
				const auto	ck = 2.0 * c * km1 - km2;
				sum += amp[ k ] * ck;
				km2 = km1;
				km1 = ck;
			}

			// One transformer: the leak is identical in both channels
			const auto	v = float ( sum ) * humVolume * wobble () * gate.getAndStep ();

			for ( auto ch = 0; ch < numChannels; ++ch )
				srcDst[ ch ][ i ] += v;
		}
	}
	//-----------------------------------------------------------------------------

	void setFrequency ( const float frequency )
	{
		phaseInc = frequency / 44100.0;
	}
	//-----------------------------------------------------------------------------

	void setVolume ( const float volume )
	{
		humVolume = fast::pow2 ( volume );
	}
	//-----------------------------------------------------------------------------

	// A generator keeps adding signal forever, so an idle chain fades it out
	// to let the output drain to true silence
	void setActive ( const bool active )
	{
		gate.set ( active ? 1.0f : 0.0f );
	}
	//-----------------------------------------------------------------------------

private:
	// 16 multiples of 50/60 Hz mains reach 800/960 Hz: the buzz band that
	// survives the speaker sim's 180 Hz highpass, and alias-free by construction
	static constexpr int	numHarmonics = 16;

	// The frozen character, tuned by ear
	static constexpr float	fundamental = 0.6f;
	static constexpr float	oddLevel = 0.5f;
	static constexpr float	tilt = 0.62f;
	static constexpr float	wobbleDepth = 0.57f * 0.3f;		// breath, ~ +-1.5 dB
	static constexpr float	flutterDepth = 0.56f * 0.15f;	// flutter, ~ +-0.7 dB

	void buildHarmonics ()
	{
		// 1/k^decay base shape; the tilt flattens the decay, which is exactly
		// what the rectifier's spiky coupling does to a real leak
		const auto	decay = fast::lerp ( 2.0f, 0.7f, tilt );

		auto	sumSq = 0.0;
		for ( auto k = 0; k < numHarmonics; ++k )
		{
			const auto	n = k + 1;	// harmonic number, n * mains Hz
			auto	a = double ( std::pow ( float ( n ), -decay ) );

			if ( n == 1 )
				a *= fundamental;
			else if ( n & 1 )
				a *= oddLevel;

			amp[ k ] = a;
			sumSq += a * a;
		}

		// RMS 1 (a cosine series has RMS^2 = sum of a^2 / 2), so the volume
		// parameter is the one and only level control
		const auto	norm = 1.0 / std::sqrt ( sumSq * 0.5 );
		for ( auto& a : amp )
			a *= norm;
	}
	//-----------------------------------------------------------------------------

	// fast::sin only holds its accuracy over [-pi/2, pi/2], so a phase in turns
	// is folded into one quadrant first, using sin's own symmetry
	[[ nodiscard ]] static fxinline float sinTurn ( const float p )
	{
		const auto	turn = p < 0.25f ? p : ( p < 0.75f ? 0.5f - p : p - 1.0f );

		return fast::sin ( turn * 2.0f * std::numbers::pi_v<float> );
	}
	//-----------------------------------------------------------------------------

	// Two amplitude layers, each a pair of incommensurate rates so the pattern
	// never audibly repeats: a sub-Hz breath (old iron under varying load) and
	// a few-Hz flutter (loose laminations rattling) riding on top
	fxinline float wobble ()
	{
		for ( auto w = 0; w < 4; ++w )
		{
			wobblePhase[ w ] += wobbleInc[ w ];
			wobblePhase[ w ] -= int ( wobblePhase[ w ] );
		}

		return 1.0f + wobbleDepth * 0.5f * ( sinTurn ( wobblePhase[ 0 ] ) + sinTurn ( wobblePhase[ 1 ] ) )
					+ flutterDepth * 0.5f * ( sinTurn ( wobblePhase[ 2 ] ) + sinTurn ( wobblePhase[ 3 ] ) );
	}

	static constexpr float	wobbleInc[ 4 ] = {	0.31f / 44100.0f, 0.127f / 44100.0f,	// breath
												3.7f / 44100.0f, 5.31f / 44100.0f };	// flutter

	double	amp[ numHarmonics ] = {};
	double	phase = 0.0;
	double	phaseInc = 50.0 / 44100.0;
	float	wobblePhase[ 4 ] = {};
	float	humVolume = 0.0025f;	// the app syncs real_hum_volume anyway
	SmoothedValue	gate { 1.0f };
};
//-----------------------------------------------------------------------------
