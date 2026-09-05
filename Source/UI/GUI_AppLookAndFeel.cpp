#include "GUI_AppLookAndFeel.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"

#include "UI/ui-colors.h"

//-------------------------------------------------------------------------------------------------

void GUI_AppLookAndFeel::drawLinearSlider ( juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider )
{
	const auto&	prop = slider.getProperties ();

	if ( slider.isBar () || style != juce::Slider::SliderStyle::LinearHorizontal || ! prop.contains ( "progress" ) )
	{
		GUI_LookAndFeel::drawLinearSlider ( g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider );
		return;
	}

	// The transport slider: the loaded part of the tune brightens the
	// background track, the value track carries the accent gradient
	const float	sliderProgress = prop.getWithDefault ( "progress", 0.0f );

	constexpr auto	trackHeight = 4.0f;

	juce::Point<float>	startPoint ( float ( x ), y + height * 0.5f );

	// Background track
	{
		juce::Point<float>	endPoint ( float ( width + x ), startPoint.y );

		juce::Path	backgroundTrack;

		backgroundTrack.startNewSubPath ( startPoint );
		backgroundTrack.lineTo ( endPoint );
		g.setColour ( UI::getShade ( sliderProgress < 1.0f ? 0.2f : 0.35f ) );
		g.strokePath ( backgroundTrack, { trackHeight, juce::PathStrokeType::curved, juce::PathStrokeType::rounded } );

		// Draw progress on top
		if ( sliderProgress < 1.0f )
		{
			juce::Point<float>	endProgressPoint ( sliderProgress * width + x, startPoint.y );

			backgroundTrack.clear ();

			backgroundTrack.startNewSubPath ( startPoint );
			backgroundTrack.lineTo ( endProgressPoint );

			g.setColour ( UI::getShade ( 0.35f ) );
			g.strokePath ( backgroundTrack, { trackHeight, juce::PathStrokeType::curved, juce::PathStrokeType::rounded } );
		}
	}

	juce::Point<float>	maxPoint { sliderPos, ( y + height ) * 0.5f };

	// Value track
	{
		juce::Path				valueTrack;
		juce::PathStrokeType	strokeType ( trackHeight, juce::PathStrokeType::curved, juce::PathStrokeType::rounded );

		valueTrack.startNewSubPath ( startPoint );
		valueTrack.lineTo ( maxPoint );

		strokeType.createStrokedPath ( valueTrack, valueTrack );

		const auto	progB = valueTrack.getBounds ();

		GUI_RoundedClip	cl ( g, progB, progB.getHeight () / 2.0f );

		g.drawImage ( progressSliderImage, progB, juce::RectanglePlacement::stretchToFit );
	}

	// Thumb
	if ( slider.isMouseOverOrDragging () )
	{
		const auto	thumbHeight = float ( getSliderThumbRadius ( slider ) );

		g.setColour ( findColour ( UI::colors::text ) );
		g.fillRoundedRectangle ( juce::Rectangle<float> ( 3.0f, thumbHeight ).withCentre ( maxPoint ), 1.5f );
	}
}
//-------------------------------------------------------------------------------------------------

void GUI_AppLookAndFeel::drawPlaybackAnimation ( juce::Graphics& g, const juce::Rectangle<float>& rect, const juce::Colour color, const float animSpeed )
{
	g.setColour ( color );

	auto	iconRect = rect;

	const auto	w = iconRect.getWidth () / 3.0f;
	const auto	h = iconRect.getHeight () * 0.8f;

	for ( auto idx = 0; idx < 3; ++idx )
		g.fillRect ( iconRect.removeFromLeft ( w ).withTrimmedRight ( 1.5f ).withTrimmedTop ( std::abs ( std::sin ( animSpeed + idx * 1.87f ) * h ) ) );
}
//-------------------------------------------------------------------------------------------------

std::vector<juce::Colour> GUI_AppLookAndFeel::oklchPalette ( juce::Colour startColor, juce::Colour endColor, const int numSteps )
{
	constexpr auto	PI = juce::MathConstants<float>::pi;

	std::vector<juce::Colour> palette;
	palette.reserve ( numSteps );

	// --- Helper: sRGB to Linear and Linear to sRGB ---
	auto toLinear = [] ( float c ) { return c <= 0.04045f ? c / 12.92f : std::pow ( ( c + 0.055f ) / 1.055f, 2.4f ); };
	auto fromLinear = [] ( float c ) { return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow ( c, 1.0f / 2.4f ) - 0.055f; };

	// --- Conversion: RGB to OkLch ---
	auto rgbToOkLch = [ & ] ( juce::Colour c ) {
		float r = toLinear ( c.getFloatRed () ), g = toLinear ( c.getFloatGreen () ), b = toLinear ( c.getFloatBlue () );
		float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
		float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
		float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
		float l_ = std::cbrt ( l ), m_ = std::cbrt ( m ), s_ = std::cbrt ( s );
		float L = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
		float a = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
		float b_ = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
		return std::vector<float>{ L, std::sqrt ( a* a + b_ * b_ ), std::atan2 ( b_, a ), c.getFloatAlpha () };
	};

	// --- Lambda: OkLch back to RGB ---
	auto oklchToRgb = [ & ] ( float L, float C, float h, float alpha ) {
		float a_comp = C * std::cos ( h ), b_comp = C * std::sin ( h );
		float l_ = L + 0.3963377774f * a_comp + 0.2158037573f * b_comp;
		float m_ = L - 0.1055613458f * a_comp - 0.0638541728f * b_comp;
		float s_ = L - 0.0894841775f * a_comp - 1.2914855480f * b_comp;
		float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
		float r = fromLinear ( +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s );
		float g = fromLinear ( -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s );
		float b = fromLinear ( -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s );
		return juce::Colour::fromFloatRGBA ( std::clamp ( r, 0.0f, 1.0f ), std::clamp ( g, 0.0f, 1.0f ), std::clamp ( b, 0.0f, 1.0f ), alpha );
	};

	auto	lch1 = rgbToOkLch ( startColor );
	auto	lch2 = rgbToOkLch ( endColor );

	auto	h1 = lch1[ 2 ];
	auto	h2 = lch2[ 2 ];
	auto	diff = h2 - h1;

	if ( diff > PI )       h1 += 2.0f * PI;
	else if ( diff < -PI ) h2 += 2.0f * PI;

	for ( auto i = 0; i < numSteps; ++i )
	{
		auto	t = i / (float)( numSteps - 1 );

		palette.push_back ( oklchToRgb (
			lch1[ 0 ] + t * ( lch2[ 0 ] - lch1[ 0 ] ), // L
			lch1[ 1 ] + t * ( lch2[ 1 ] - lch1[ 1 ] ), // C
			h1 + t * ( h2 - h1 ),                // h
			lch1[ 3 ] + t * ( lch2[ 3 ] - lch1[ 3 ] )  // alpha
		) );
	}

	return palette;
}
//-------------------------------------------------------------------------------------------------

juce::ColourGradient GUI_AppLookAndFeel::withOklchStops ( const juce::ColourGradient& gradient, const int stepsPerSegment )
{
	auto	bridged = gradient;

	for ( auto i = 0; i < gradient.getNumColours () - 1; ++i )
	{
		const auto	p1 = gradient.getColourPosition ( i );
		const auto	p2 = gradient.getColourPosition ( i + 1 );

		const auto	pal = oklchPalette ( gradient.getColour ( i ), gradient.getColour ( i + 1 ), stepsPerSegment + 2 );

		for ( auto step = 1; step <= stepsPerSegment; ++step )
			bridged.addColour ( p1 + ( p2 - p1 ) * double ( step ) / double ( stepsPerSegment + 1 ), pal[ size_t ( step ) ] );
	}

	return bridged;
}
//-------------------------------------------------------------------------------------------------

void GUI_AppLookAndFeel::updateProgressColors ()
{
	auto& g = progressSlider;

	g.clearColours ();

	const auto	col1 = findColour ( UI::colors::accent );
	const auto	col2 = findColour ( UI::colors::accent2 );

	g.addColour ( 0.0, col1 );
	g.addColour ( 1.0, col2 );

	const auto	pal = oklchPalette ( col1, col2, 16 );
	const auto	lerpInc = 1.0f / pal.size ();
	for ( auto i = 0.0f; const auto c : pal )
	{
		g.addColour ( double ( i ) * 0.2 + 0.8, c );
		i += lerpInc;
	}

	// Create image for slider progress (used in drawLinearSlider)
	progressSliderImage = juce::Image ( juce::Image::RGB, 200, 4, false );
	{
		juce::Graphics	ig ( progressSliderImage );

		g.point1 = { 0.0f, 0.0f };
		g.point2 = { float ( progressSliderImage.getWidth () ), 0.0f };

		ig.setGradientFill ( g );
		ig.fillAll ();
	}
}
//-------------------------------------------------------------------------------------------------
