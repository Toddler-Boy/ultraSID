#pragma once

#include <vector>

//
// R128 integrated loudness plus the SID midband energy measurement, two
// values per tune: the player composes the rating from them. The filter
// constants bake into the stored numbers, changing them means re-measuring
// the library; the punishment knobs do not.
//
class PerceivedLoudness
{
public:
	// The punishment knobs, calibrated by ear on the David Whittaker corpus
	static constexpr double	defaultMidK = 8.0;
	static constexpr double	defaultMidBaseline = 0.15;

	PerceivedLoudness ( double sampleRate, int numChannels,
						double midK = defaultMidK, double midBaseline = defaultMidBaseline );

	// The rating every playback path uses: integrated loudness plus the
	// midband punishment, composed from the two stored measurements
	[[ nodiscard ]] static double composeRating ( double integrated, double mid,
												  double midK = defaultMidK, double midBaseline = defaultMidBaseline );

	void	reset ();
	void	process ( const float* const* channels, int numFrames );

	// The two stored measurements, queryable mid-render: integrated loudness,
	// and the loudness of the midband-filtered signal (ungated)
	[[ nodiscard ]] double	integratedLUFS () const;
	[[ nodiscard ]] double	midLUFS () const;

	// integratedLUFS plus the midband punishment (the live-measurement rating)
	[[ nodiscard ]] double	effectiveLUFS () const;

	// Calibration readout
	[[ nodiscard ]] float	samplePeak () const;

private:
	// Stage 1, lanes 0-3: K shelf L/R and mid bandpass L/R off the same raw
	// input; stage 2: K highpass on the shelf pair. The fixed-bound lane
	// loops are what clang vectorizes, keep the structure
	double	s1B0[ 4 ], s1B1[ 4 ], s1B2[ 4 ], s1A1[ 4 ], s1A2[ 4 ];
	double	s1Z1[ 4 ], s1Z2[ 4 ];
	double	hpA1, hpA2;
	double	hpZ1[ 2 ], hpZ2[ 2 ];

	std::vector<double>	subBlocks;	// channel-summed K mean-square per 100 ms
	double	subK = 0.0, subM = 0.0;
	double	totalM = 0.0;
	int		subPos = 0;
	float	peak = 0.0f;

	int		subLen;
	bool	stereo;
	double	midK, midBaseline;
};
//-----------------------------------------------------------------------------
