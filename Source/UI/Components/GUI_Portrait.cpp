#include <JuceHeader.h>

#include "GUI_Portrait.h"

#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/UI/Components/GUI_RoundedClip.h"

//-----------------------------------------------------------------------------

constexpr auto	cycleDurationMs = 5000.0;
constexpr auto	sweepDurationMs = 700.0;

GUI_Portrait::GUI_Portrait ()
{
	setName ( "portrait" );
	setInterceptsMouseClicks ( false, false );

	// Gold for profiles with certication
	{
		goldGradient = juce::ColourGradient::vertical ( juce::Colour ( 0xFF6A4E0B ), 0.0f, juce::Colour ( 0xFF6A4E0B ), 100.0f );

		goldGradient.addColour ( 0.00, juce::Colour ( 0xFF996515 ) ); // Warm golden brown
		goldGradient.addColour ( 0.25, juce::Colour ( 0xFFF3E5AB ) ); // Vanilla highlight reflection
		goldGradient.addColour ( 0.50, juce::Colour ( 0xFFD4AF37 ) ); // Metallic Mid-Gold
		goldGradient.addColour ( 0.75, juce::Colour ( 0xFFE6C65B ) ); // Pale reflective gold
		goldGradient.addColour ( 1.00, juce::Colour ( 0xFFB8860B ) ); // Dark Gold
	}

	// Silver for uncertified profiles
	{
		silverGradient = juce::ColourGradient::vertical ( juce::Colour ( 0xFF505050 ), 0.0f, juce::Colour ( 0xFF505050 ), 100.0f );

		silverGradient.addColour ( 0.00, juce::Colour ( 0xFF808080 ) ); // Slate grey
		silverGradient.addColour ( 0.25, juce::Colour ( 0xFFDCDCDC ) ); // Bright Gainsboro silver
		silverGradient.addColour ( 0.50, juce::Colour ( 0xFFA9A9A9 ) ); // Matte Mid-Silver
		silverGradient.addColour ( 0.75, juce::Colour ( 0xFFF5F5F5 ) ); // Soft white-silver reflection
		silverGradient.addColour ( 1.00, juce::Colour ( 0xFF707070 ) ); // Dim charcoal grey
	}

	// White sheen
	{
		whiteSheenGradient = juce::ColourGradient::vertical ( juce::Colours::transparentWhite, 0.0f, juce::Colours::transparentWhite, 100.0f );

		whiteSheenGradient.addColour ( 0.35, juce::Colours::transparentWhite );
		whiteSheenGradient.addColour ( 0.60, juce::Colours::white );
		whiteSheenGradient.addColour ( 0.65, juce::Colours::white );
		whiteSheenGradient.addColour ( 0.75, juce::Colours::transparentWhite );
	}
}
//-----------------------------------------------------------------------------

void GUI_Portrait::resized ()
{
	const auto	b = getLocalBounds ().toFloat ();

	silverGradient.point1 = b.getTopLeft ();
	silverGradient.point2 = b.getBottomRight ();

	goldGradient.point1 = silverGradient.point1;
	goldGradient.point2 = silverGradient.point2;

	whiteSheenGradient.point1 = silverGradient.point1;
	whiteSheenGradient.point2 = silverGradient.point2;
}
//-----------------------------------------------------------------------------

void GUI_Portrait::paint ( juce::Graphics& g )
{
	// No bitmap given
	if ( bitmap.isEmpty () )
		return;

	const auto	b = getLocalBounds ().toFloat ();

	// Bitmap given, draw it
	if ( bitmap.startsWithIgnoreCase ( "emu-" ) )
	{
		mipMap.draw ( g, b );
		return;
	}

	// Author portrait
	auto drawPortrait = [ &g, &b, this ] ( const float opacity )
	{
		const auto	pb = b.reduced ( 2.0f );
		const auto	gs = GUI_RoundedClip ( g, pb, 100'000.0f );

		g.setOpacity ( opacity );
		mipMap.draw ( g, pb, juce::RectanglePlacement::fillDestination );
	};

	// Silver or Gold gradient
	{
		const auto	gs = GUI_RoundedClip ( g, b, 100'000.0f );

		g.setGradientFill ( useGoldenBorder ? goldGradient : silverGradient );
		g.fillAll ();

		drawPortrait ( 1.0f );

		// White sheen animation
		{
			const auto	timeMs = juce::Time::getMillisecondCounterHiRes ();
			const auto	modTime = std::fmod ( timeMs, cycleDurationMs );

			auto	sweepPhase = 0.0f;
			if ( modTime < sweepDurationMs )
				sweepPhase = float ( modTime / sweepDurationMs );

			const auto	currentXOffset = -b.getWidth () + sweepPhase * b.getWidth () * 2;

			{
				const auto	state = juce::Graphics::ScopedSaveState ( g );

				g.addTransform ( juce::AffineTransform::translation ( currentXOffset, currentXOffset ) );
				g.setGradientFill ( whiteSheenGradient );
				g.fillAll ();
			}
		}

		drawPortrait ( 0.65f );
	}
}
//-----------------------------------------------------------------------------

void GUI_Portrait::setBitmap ( const juce::String& _bitmap, const bool _useGoldenBorder )
{
	bitmap = _bitmap;
	useGoldenBorder = _useGoldenBorder;

	if ( bitmap.isEmpty () )
		mipMap = {};
	else
	{
		const auto	isEmuProfile = bitmap.startsWithIgnoreCase ( "emu-" );
		const auto	folder = isEmuProfile ? "UI/png/" : "Portraits/Musicians/";
		const auto	suffix = isEmuProfile ? ".png" : ".jpg";

		if ( isEmuProfile )
			bitmap = bitmap.replaceCharacter ( ' ', '_' );

		mipMap.setImage ( datasource::loadImage ( folder + bitmap + suffix ) );
	}

	setVisible ( mipMap.isValid () );
	if ( mipMap.isValid () )
		repaint ();
}
//-----------------------------------------------------------------------------

void GUI_Portrait::update ()
{
	if ( ! isVisible () || bitmap.isEmpty () || bitmap.startsWithIgnoreCase ( "emu-" ) )
		return;

	const auto	timeMs = juce::Time::getMillisecondCounterHiRes ();
	const auto	modTime = std::fmod ( timeMs, cycleDurationMs );

	if ( modTime < sweepDurationMs )
		repaint ();
}
//-----------------------------------------------------------------------------
