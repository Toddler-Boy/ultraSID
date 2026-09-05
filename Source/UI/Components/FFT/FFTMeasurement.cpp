#include <algorithm>

#include "FFTMeasurement.h"

#include "fft-helpers.h"
#include "pffft/pffft.h"

//-----------------------------------------------------------------------------

FFTMeasurement::FFTMeasurement ()
{
	static_assert ( FFT_SIZE % 32 == 0, "pffft real transforms need a multiple of 32" );
	fftSetup = pffft_new_setup ( FFT_SIZE, PFFFT_REAL );

	fft::helpers::window_Hann ( { window, FFT_SIZE } );

	// pffft doesn't scale the transform; fold the 1/N normalization into the window
	for ( auto& w : window )
		w *= 1.0f / FFT_SIZE;
}
//-----------------------------------------------------------------------------

FFTMeasurement::~FFTMeasurement ()
{
	pffft_destroy_setup ( fftSetup );
}
//-----------------------------------------------------------------------------

int FFTMeasurement::maxBin ()
{
	static const auto	bin = std::min ( freqToBin ( UI::fft::fftFreqStop, FFT_SIZE ) + 2, FFT_SIZE / 2 - 1 );

	return bin;
}
//-----------------------------------------------------------------------------

void FFTMeasurement::reset ()
{
	circularOffset = 0;
	counterSinceLastUpdate = 0;

	std::ranges::fill ( circularBuffer, 0.0f );

	std::ranges::fill ( fftData, 0.0f );
	std::ranges::fill ( srcData, 0.0f );
	std::ranges::fill ( dbData, 0.0f );
}
//-----------------------------------------------------------------------------

bool FFTMeasurement::update ()
{
	if ( auto expected = true; ! needsUpdate.compare_exchange_strong ( expected, false ) )
		return false;

	// Transform to frequency domain: interleaved re/im pairs per bin,
	// with DC and Nyquist packed together into the first pair
	pffft_transform_ordered ( fftSetup, srcData, fftData, fftWork, PFFFT_FORWARD );

	constexpr auto POW2 = [] ( const auto value ) -> float { return value * value; };

	constexpr auto	decay = 2.0f;
	for ( auto i = 1; i < maxBin (); ++i )
	{
		// Get magnitude of bin i; 10 * log10 ( mag² ) == 20 * log10 ( mag ), skipping the sqrt
		const auto	magSq = POW2 ( fftData[ i * 2 ] ) + POW2 ( fftData[ i * 2 + 1 ] );
		const auto	db = std::max ( 0.0f, 10.0f * std::log10 ( magSq ) + 80.0f );

		if ( dbData[ i ] > -100.0f )
			dbData[ i ] -= decay;

		dbData[ i ] = std::max ( dbData[ i ], db );
	}

	return true;
}
//-----------------------------------------------------------------------------

void FFTMeasurement::pushAudio ( const float* src, int sampleFrames )
{
	auto	srcOffset = 0;

	// Copy new data into circular buffer
	if ( circularOffset >= FFT_SIZE )
		circularOffset = 0;

	while ( sampleFrames )
	{
		const auto	toCopy = std::min ( sampleFrames, FFT_SIZE - circularOffset );
		if ( toCopy )
		{
			std::copy_n ( src + srcOffset, toCopy, circularBuffer + circularOffset );

			sampleFrames -= toCopy;
			circularOffset += toCopy;
			srcOffset += toCopy;

			if ( circularOffset >= FFT_SIZE )
				circularOffset = 0;

			constexpr auto	fftRate = 44100 / 60;

			counterSinceLastUpdate += toCopy;
			if ( counterSinceLastUpdate >= fftRate )
			{
				counterSinceLastUpdate -= fftRate;

				// Window the ring in chronological order: oldest samples start at
				// circularOffset and wrap, newest end just before it
				const auto	tail = FFT_SIZE - circularOffset;

				fft::helpers::applyWindow ( { srcData, size_t ( tail ) },
											{ circularBuffer + circularOffset, size_t ( tail ) },
											{ window, size_t ( tail ) } );
				fft::helpers::applyWindow ( { srcData + tail, size_t ( circularOffset ) },
											{ circularBuffer, size_t ( circularOffset ) },
											{ window + tail, size_t ( circularOffset ) } );

				needsUpdate = true;
			}
		}
	}
}
//-----------------------------------------------------------------------------
