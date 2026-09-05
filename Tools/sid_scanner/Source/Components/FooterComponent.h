#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Footer status strip: queue remaining, elapsed time, and a total-progress bar
// with the estimated time left

class FooterComponent final : public juce::Component
{
public:
	FooterComponent () = default;

	struct Stats
	{
		int		remaining = 0;		// entries still pending or running
		int		total = 0;			// all entries in the queue
		double	fractionDone = 0.0;	// total progress, 0..1
		double	speedRatio = -1.0;		// rendered length vs wall time per tune, < 0 while unknown
		double	totalSpeedRatio = 0.0;	// speedRatio times the currently active workers
		int64_t	elapsedMs = 0;		// how long the work has been going on
		int64_t	totalTimeMs = -1;	// approximate total duration, < 0 while unknown
	};

	void setStats ( const Stats& newStats );

	void paint ( juce::Graphics& g ) override;

private:
	Stats		stats;

	// Rounded outline of the progress bar, used as a clip while painting; built at
	// origin since the bar's x position depends on the text cells before it, and
	// rebuilt on the fly whenever the bar height changes
	juce::Path	barPath;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( FooterComponent )
};
//-----------------------------------------------------------------------------
