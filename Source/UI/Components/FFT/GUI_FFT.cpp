#include "GUI_FFT.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "fft-helpers.h"

//-----------------------------------------------------------------------------

GUI_FFT::GUI_FFT ()
{
	// Bin 0 is 0 Hz, whose log-scale position is undefined; it is never drawn
	for ( auto i = 1; i < FFTMeasurement::FFT_SIZE / 2; ++i )
		normX[ i ] = UI::fft::freqToNormalized ( FFTMeasurement::binToFreq ( i, FFTMeasurement::FFT_SIZE ) );

	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_FFT::rebuildGradient ()
{
	const auto	bentoCol = findColour ( UI::bento, true );
	const auto	fillCol = findColour ( fillColId, true );

	// A transparent theme color skips the fill entirely
	fillVisible = ! fillCol.isTransparent ();

	fillGradient = juce::ColourGradient::vertical ( bentoCol.interpolatedWith ( fillCol, brightness ),
													bentoCol.interpolatedWith ( fillCol, 0.04f * brightness ),
													getLocalBounds ().toFloat () );
}
//-----------------------------------------------------------------------------

void GUI_FFT::paint ( juce::Graphics& g )
{
	auto	font = UI::font ( UI::fonts::fft_caption );
	g.setFont ( font );

	const auto	bentoCol = findColour ( UI::bento );

	//
	// Draw FFT
	//
	{
		const auto	fftR = getLocalBounds ().toFloat ().withTrimmedBottom ( 6.0f );
		auto	sg = GUI_RoundedClip ( g, fftR, UI::corner ( UI::corners::fft_clip, fftR ) );

		const auto&	path = mirroredPath ? *mirroredPath : fftPath;

		if ( fillVisible && ! path.isEmpty () )
		{
			g.setGradientFill ( fillGradient );
			g.fillPath ( path );
		}

		const auto	lineCol = findColour ( lineColId, true );

		if ( const auto stroke = lineStroke ();
			 ! lineCol.isTransparent () && ! path.isEmpty () && UI::lines::visible ( stroke.getStrokeThickness () ) )
		{
			g.setColour ( bentoCol.interpolatedWith ( lineCol, brightness ) );
			g.strokePath ( path, stroke );
		}
	}
}
//-----------------------------------------------------------------------------

juce::PathStrokeType GUI_FFT::lineStroke ()
{
	return { UI::lineWidth ( UI::lines::fft_curve ), juce::PathStrokeType::curved, juce::PathStrokeType::butt };
}
//-----------------------------------------------------------------------------

void GUI_FFT::reset ()
{
	fftPath.clear ();
	mirroredPath = nullptr;
}
//-----------------------------------------------------------------------------

void GUI_FFT::setBrightness ( const float _brightness )
{
	brightness = _brightness;

	rebuildGradient ();
}
//-----------------------------------------------------------------------------

// Rebuilds the curve path from the source's current levels
void GUI_FFT::update ()
{
	if ( ! source )
		return;

	// Back to painting this display's own path
	mirroredPath = nullptr;

	const auto	levels = source->levels ();
	const auto	maxBin = int ( levels.size () );

	// Create path
	{
		fftPath.clear ();

		const auto	width = float ( getWidth () );
		const auto	height = float ( getHeight () - 6 );

		const auto	dbToY = [ height ] ( const float db )
		{
			return ( std::clamp ( -db, -80.0f, 0.0f ) / 80.0f + 1.0f ) * height;
		};

		// Overhang for path segments that must stay outside the clip region; the
		// stroke (with rounded joins) reaches half its thickness past the path
		const auto	over = std::max ( 2.0f, lineStroke ().getStrokeThickness () );

		auto	minY = std::numeric_limits<float>::max ();
		auto	lastX = -over;

		for ( auto i = 1; i < maxBin && lastX <= width; ++i )
		{
			const auto	x = normX[ i ] * width;

			minY = std::min ( minY, dbToY ( levels[ i ] ) );

			if ( ( x - lastX ) < pixDelta && i != ( maxBin - 1 ) )
				continue;

			const auto	y = std::min ( minY, height - 0.5f ) + 1.5f;

			if ( lastX <= 0.0f )
				fftPath.startNewSubPath ( lastX, y );
			else
				fftPath.lineTo ( x, y );

			lastX = x;
			minY = std::numeric_limits<float>::max ();
		}

		// Round the curve's corners; near-zero values skip the whole path rebuild
		if ( const auto rounding = UI::corner ( UI::corners::fft_curve ); rounding >= 0.1f )
			fftPath = fftPath.createPathWithRoundedCorners ( rounding );

		// Close the path outside the clip region (right, below, left) so paint()
		// can stroke and fill the same path; the closing segments never show
		if ( ! fftPath.isEmpty () )
		{
			const auto	yBottom = float ( getHeight () - 6 ) + over;

			fftPath.lineTo ( width + over, fftPath.getCurrentPosition ().y );
			fftPath.lineTo ( width + over, yBottom );
			fftPath.lineTo ( -over, yBottom );
			fftPath.closeSubPath ();
		}
	}

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_FFT::mirror ( const GUI_FFT& source )
{
	mirroredPath = &source.fftPath;

	repaint ();
}
//------------------------------------------------------------------------------
