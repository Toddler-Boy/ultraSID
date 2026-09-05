#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <span>

struct PFFFT_Setup;

//-----------------------------------------------------------------------------

// The analysis half of the FFT displays, one instance per channel: the audio
// thread feeds pushAudio, the UI timer runs update, displays read levels

class FFTMeasurement final
{
public:
	FFTMeasurement ();
	~FFTMeasurement ();

	void reset ();

	// Runs the transform once fresh audio has been windowed; true = levels changed
	bool update ();

	void pushAudio ( const float* src, int sampleFrames );

	// Per-bin dB above the -80 dB floor (0..80); bin 0 holds DC/Nyquist packed
	// and is never valid
	[[ nodiscard ]] std::span<const float> levels () const	{	return { dbData, size_t ( maxBin () ) };	}

	static inline constexpr auto	FFT_SIZE = 8192;
	static inline constexpr auto	sampleRate = 44'100.0f;

	[[ nodiscard ]] static inline float binToFreq ( const int bin, const int fftSize )
	{
		return float ( bin ) / float ( fftSize ) * sampleRate;
	}

	[[ nodiscard ]] static inline int freqToBin ( const float freq, const int fftSize )
	{
		return int ( freq / sampleRate * float ( fftSize ) );
	}

	// Bins above the displayed range (UI::fft::fftFreqStop) are never computed
	[[ nodiscard ]] static int maxBin ();

private:
	int		circularOffset = 0;
	int		counterSinceLastUpdate = 0;
	alignas ( 16 ) float	circularBuffer[ FFT_SIZE ] = {};

	// pffft needs its in/out/work buffers 16-byte aligned
	alignas ( 16 ) float	srcData[ FFT_SIZE ] = {};
	alignas ( 16 ) float	fftData[ FFT_SIZE ] = {};
	alignas ( 16 ) float	fftWork[ FFT_SIZE ] = {};

	alignas ( 16 ) float	window[ FFT_SIZE ] = {};
	alignas ( 16 ) float	dbData[ FFT_SIZE ] = {};

	PFFFT_Setup*	fftSetup = nullptr;

	std::atomic<bool>	needsUpdate = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( FFTMeasurement )
};
//-----------------------------------------------------------------------------
