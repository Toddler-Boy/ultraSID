#include <algorithm>
#include <cmath>

#include "PerceivedLoudness.h"

// The K filters come from the BS.1770 analog prototypes, bilinear-transformed
// at the actual sample rate: reproduces the spec's 48 kHz coefficient table
// exactly and stays exact at 44.1 kHz
static constexpr auto	shelfF0 = 1681.9744509555319;
static constexpr auto	shelfGainDb = 3.99984385397;
static constexpr auto	shelfQ = 0.7071752369554193;
static constexpr auto	shelfVbExp = 0.499666774155;
static constexpr auto	highpassF0 = 38.13547087613982;
static constexpr auto	highpassQ = 0.5003270373253953;

// The midband approximates the ISO 226 2-4 kHz sensitivity bump that
// K-weighting under-represents (why resonant middy tunes sound louder than
// they measure). Changing f0 or Q invalidates every stored ratio
static constexpr auto	midF0 = 2800.0;
static constexpr auto	midQ = 0.6;

static constexpr auto	pi = 3.141592653589793;

//-----------------------------------------------------------------------------

PerceivedLoudness::PerceivedLoudness ( const double sampleRate, const int numChannels,
				   const double midK, const double midBaseline )
	: subLen ( int ( sampleRate * 0.1 + 0.5 ) )
	, stereo ( numChannels == 2 )
	, midK ( midK ), midBaseline ( midBaseline )
{
	const auto	sk = std::tan ( pi * shelfF0 / sampleRate );
	const auto	vh = std::pow ( 10.0, shelfGainDb / 20.0 );
	const auto	vb = std::pow ( vh, shelfVbExp );
	const auto	shelfNorm = 1.0 / ( 1.0 + sk / shelfQ + sk * sk );

	const auto	mk = std::tan ( pi * midF0 / sampleRate );
	const auto	midNorm = 1.0 / ( 1.0 + mk / midQ + mk * mk );

	for ( auto lane = 0; lane < 2; ++lane )
	{
		s1B0[ lane ] = ( vh + vb * sk / shelfQ + sk * sk ) * shelfNorm;
		s1B1[ lane ] = 2.0 * ( sk * sk - vh ) * shelfNorm;
		s1B2[ lane ] = ( vh - vb * sk / shelfQ + sk * sk ) * shelfNorm;
		s1A1[ lane ] = 2.0 * ( sk * sk - 1.0 ) * shelfNorm;
		s1A2[ lane ] = ( 1.0 - sk / shelfQ + sk * sk ) * shelfNorm;

		// Unity-peak-gain bandpass
		s1B0[ lane + 2 ] = mk / midQ * midNorm;
		s1B1[ lane + 2 ] = 0.0;
		s1B2[ lane + 2 ] = -mk / midQ * midNorm;
		s1A1[ lane + 2 ] = 2.0 * ( mk * mk - 1.0 ) * midNorm;
		s1A2[ lane + 2 ] = ( 1.0 - mk / midQ + mk * mk ) * midNorm;
	}

	const auto	hk = std::tan ( pi * highpassF0 / sampleRate );
	const auto	hpNorm = 1.0 / ( 1.0 + hk / highpassQ + hk * hk );
	hpA1 = 2.0 * ( hk * hk - 1.0 ) * hpNorm;
	hpA2 = ( 1.0 - hk / highpassQ + hk * hk ) * hpNorm;

	reset ();
}
//-----------------------------------------------------------------------------

void PerceivedLoudness::reset ()
{
	std::fill ( std::begin ( s1Z1 ), std::end ( s1Z1 ), 0.0 );
	std::fill ( std::begin ( s1Z2 ), std::end ( s1Z2 ), 0.0 );
	hpZ1[ 0 ] = hpZ1[ 1 ] = hpZ2[ 0 ] = hpZ2[ 1 ] = 0.0;

	subBlocks.clear ();
	subK = subM = totalM = 0.0;
	subPos = 0;
	peak = 0.0f;
}
//-----------------------------------------------------------------------------

void PerceivedLoudness::process ( const float* const* channels, const int numFrames )
{
	// Mono runs through both lanes: the summed accumulation then equals the
	// EBU dual mono convention (channel weight 2.0)
	const auto*	left = channels[ 0 ];
	const auto*	right = stereo ? channels[ 1 ] : channels[ 0 ];

	for ( auto i = 0; i < numFrames; ++i )
	{
		const double	inL = left[ i ];
		const double	inR = right[ i ];

		// Stage 1 off the raw input: K shelf (lanes 0/1) and mid bandpass
		// (lanes 2/3). The bandpass deliberately taps raw, not K-weighted,
		// audio; the ratio baseline is tuned under this same structure
		const double	x1[ 4 ] = { inL, inR, inL, inR };
		double			y1[ 4 ];

		for ( auto lane = 0; lane < 4; ++lane )
		{
			const auto	x = x1[ lane ];
			const auto	y = s1B0[ lane ] * x + s1Z1[ lane ];
			s1Z1[ lane ] = s1B1[ lane ] * x - s1A1[ lane ] * y + s1Z2[ lane ];
			s1Z2[ lane ] = s1B2[ lane ] * x - s1A2[ lane ] * y;
			y1[ lane ] = y;
		}

		// Stage 2: K highpass on the shelf pair, numerator fixed { 1, -2, 1 }
		double	y2[ 2 ];

		for ( auto lane = 0; lane < 2; ++lane )
		{
			const auto	x = y1[ lane ];
			const auto	y = x + hpZ1[ lane ];
			hpZ1[ lane ] = -2.0 * x - hpA1 * y + hpZ2[ lane ];
			hpZ2[ lane ] = x - hpA2 * y;
			y2[ lane ] = y;
		}

		subK += y2[ 0 ] * y2[ 0 ] + y2[ 1 ] * y2[ 1 ];
		subM += y1[ 2 ] * y1[ 2 ] + y1[ 3 ] * y1[ 3 ];
		peak = std::max ( { peak, std::abs ( left[ i ] ), std::abs ( right[ i ] ) } );

		if ( ++subPos == subLen )
		{
			subBlocks.push_back ( subK / subLen );
			totalM += subM;
			subK = subM = 0.0;
			subPos = 0;
		}
	}
}
//-----------------------------------------------------------------------------

double PerceivedLoudness::integratedLUFS () const
{
	// 400 ms gating blocks at 100 ms hop, each the mean of 4 sub-blocks
	const auto	numBlocks = int ( subBlocks.size () ) - 3;
	if ( numBlocks < 1 )
		return -HUGE_VAL;

	const auto	block = [ this ] ( const int j )
	{
		return ( subBlocks[ j ] + subBlocks[ j + 1 ] + subBlocks[ j + 2 ] + subBlocks[ j + 3 ] ) * 0.25;
	};

	// Absolute gate at -70 LUFS, then re-average above the relative gate
	// 10 LU under the surviving mean (two O(n) passes per query)
	const auto	absGate = std::pow ( 10.0, ( -70.0 + 0.691 ) / 10.0 );

	auto	sum = 0.0;
	auto	count = 0;
	for ( auto j = 0; j < numBlocks; ++j )
	{
		const auto	z = block ( j );
		if ( z > absGate )
		{
			sum += z;
			++count;
		}
	}
	if ( ! count )
		return -HUGE_VAL;

	const auto	relGate = std::max ( absGate, sum / count * 0.1 );

	sum = 0.0;
	count = 0;
	for ( auto j = 0; j < numBlocks; ++j )
	{
		const auto	z = block ( j );
		if ( z > relGate )
		{
			sum += z;
			++count;
		}
	}
	if ( ! count )
		return -HUGE_VAL;

	return -0.691 + 10.0 * std::log10 ( sum / count );
}
//-----------------------------------------------------------------------------

double PerceivedLoudness::midLUFS () const
{
	if ( subBlocks.empty () || totalM <= 0.0 )
		return -HUGE_VAL;

	return -0.691 + 10.0 * std::log10 ( totalM / ( double ( subBlocks.size () ) * subLen ) );
}
//-----------------------------------------------------------------------------

float PerceivedLoudness::samplePeak () const
{
	return peak;
}
//-----------------------------------------------------------------------------

double PerceivedLoudness::composeRating ( const double integrated, const double mid,
										  const double midK, const double midBaseline )
{
	if ( integrated == -HUGE_VAL )
		return -HUGE_VAL;

	if ( mid <= -96.0 )
		return integrated;

	// Thin middy tunes measure quieter than they sound: raise the rating by
	// the mid energy excess over the neutral baseline
	const auto	midShare = std::pow ( 10.0, ( mid - integrated ) * 0.1 );

	return integrated + midK * std::max ( 0.0, midShare - midBaseline );
}
//-----------------------------------------------------------------------------

double PerceivedLoudness::effectiveLUFS () const
{
	return composeRating ( integratedLUFS (), midLUFS (), midK, midBaseline );
}
//-----------------------------------------------------------------------------
