#include <fmt/format.h>

#include "GUI_EQCurve.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "Audio/SIDEffects.h"
#include "Helpers/Messages.h"
#include "UI/Components/FFT/fft-helpers.h"
#include "UI/Components/FFT/FFTMeasurement.h"
#include "UI/GUI_AppLookAndFeel.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

namespace
{
	constexpr const char*	bandNames[ 3 ] = { "low", "mid", "high" };

	constexpr float	boxBlend = 0.067f;		// the GUI_SettingsBox fill the curve sits on

	constexpr float	handleGap = 2.0f;		// clearance left at full deflection
	constexpr float	hoverGrow = 3.0f;		// dot growth under the mouse

	constexpr float	dashWidth = 1.0f;
	constexpr float	barGap = 1.0f;			// between spectrum bars
	constexpr int	labelHeight = 12;		// band name strip at the bottom

	// The square hit box wraps the hovered dot plus its halo
	[[ nodiscard ]] float handleSize ()
	{
		return UI::lineWidth ( UI::lines::eq_dot ) + hoverGrow + UI::lineWidth ( UI::lines::eq_halo ) * 2.0f;
	}

	[[ nodiscard ]] float handleMargin ()
	{
		return handleSize () * 0.5f + handleGap;
	}

	// Three equal-width bands with the handle centered in each: a plain
	// low/mid/high representation, deliberately not a frequency axis
	[[ nodiscard ]] float bandToX ( const int band, const float width )
	{
		return ( float ( band ) * 2.0f + 1.0f ) / 6.0f * width;
	}
}
//-----------------------------------------------------------------------------

GUI_EQCurve::GUI_EQCurve ()
{
	setInterceptsMouseClicks ( false, true );

	addAndMakeVisible ( backdrop );
	addAndMakeVisible ( spectrum );
	addAndMakeVisible ( curve );
}
//-----------------------------------------------------------------------------

// The backdrop fills everything, spectrum and curve get the part above the labels
void GUI_EQCurve::resized ()
{
	backdrop.setBounds ( getLocalBounds () );
	spectrum.setBounds ( getLocalBounds ().withTrimmedBottom ( labelHeight ) );
	curve.setBounds ( getLocalBounds ().withTrimmedBottom ( labelHeight ) );
}
//-----------------------------------------------------------------------------

GUI_EQCurve::Backdrop::Backdrop ()
{
	setInterceptsMouseClicks ( false, false );
	setBufferedToImage ( true );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Backdrop::paint ( juce::Graphics& g )
{
	const auto	w = float ( getWidth () );
	const auto	labelTop = float ( getHeight () ) - float ( labelHeight );
	const auto	dashBottom = labelTop - 2.0f;

	// Shades are a lift above the settings box fill, scaled by the themed
	// visibility: 0 hides an element, 1 is the stock look
	const auto	columnVis = UI::paddingDef ( UI::paddings::eq_column_visibility ).top;
	const auto	dashVis = UI::paddingDef ( UI::paddings::eq_dash_visibility ).top;

	auto	column = juce::ColourGradient::vertical ( UI::getShade ( boxBlend ), 0.0f,
													  UI::getShade ( boxBlend ), labelTop );
	column.addColour ( 0.5, UI::getShade ( boxBlend + 0.05f * columnVis ) );

	auto	dashFade = juce::ColourGradient::vertical ( UI::getShade ( boxBlend + 0.025f * dashVis ), 0.0f,
														UI::getShade ( boxBlend + 0.025f * dashVis ), labelTop );
	dashFade.addColour ( 0.5, UI::getShade ( boxBlend + 0.125f * dashVis ) );

	// The middle band gets a soft column, swelling towards the curve's center
	{
		g.setGradientFill ( column );
		g.fillRect ( juce::Rectangle<float> ( w / 3.0f, 0.0f, w / 3.0f, labelTop ) );
	}

	// One dashed center line per band
	{
		constexpr float	dashPattern[ 2 ] = { 3.0f, 3.0f };

		// One line at a time, createDashedStroke dashes the subpath jumps too
		juce::Path	dashes;

		for ( auto band = 0; band < 3; ++band )
		{
			juce::Path	center, dashed;

			center.startNewSubPath ( bandToX ( band, w ), 0.0f );
			center.lineTo ( bandToX ( band, w ), dashBottom );

			juce::PathStrokeType ( dashWidth ).createDashedStroke ( dashed, center, dashPattern, 2 );
			dashes.addPath ( dashed );
		}

		g.setGradientFill ( dashFade );
		g.fillPath ( dashes );
	}

	g.setFont ( UI::font ( UI::fonts::eq_label ) );
	g.setColour ( findColour ( UI::colors::textMuted ) );

	for ( auto band = 0; band < 3; ++band )
		g.drawText ( strings->get ( juce::String ( "settings/eq/" ) + bandNames[ band ] ),
					 juce::Rectangle<float> ( w / 3.0f * float ( band ), labelTop, w / 3.0f, float ( labelHeight ) ),
					 juce::Justification::centred, false );
}
//-----------------------------------------------------------------------------

GUI_EQCurve::Spectrum::Spectrum ()
{
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

// The extremes run hot, the quiet center cold
void GUI_EQCurve::Spectrum::rebuildGradient ()
{
	const auto	h = float ( getHeight () );

	juce::ColourGradient	anchors ( findColour ( UI::colors::eq_hot ), 0.0f, 0.0f,
									  findColour ( UI::colors::eq_hot ), 0.0f, h, false );
	anchors.addColour ( 0.25, findColour ( UI::colors::eq_neutral ) );
	anchors.addColour ( 0.5, findColour ( UI::colors::eq_cold ) );
	anchors.addColour ( 0.75, findColour ( UI::colors::eq_neutral ) );

	gradient = GUI_AppLookAndFeel::withOklchStops ( anchors );
	gradient.multiplyOpacity ( UI::paddingDef ( UI::paddings::eq_bars_alpha ).top );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Spectrum::paint ( juce::Graphics& g )
{
	constexpr auto	maxBars = 128;

	const auto	numBars = std::min ( int ( UI::lineWidth ( UI::lines::eq_bars ) ), maxBars );

	if ( ! left || numBars < 1 )
		return;

	// Per-bin x positions, piecewise log-frequency, warped so the EQ band
	// frequencies land on the visual band centers
	static const auto	normX = []
	{
		std::array<float, FFTMeasurement::FFT_SIZE / 2>	table {};

		const float	ctrlLog[ 5 ] = {	std::log10 ( UI::fft::fftFreqStart ),
										std::log10 ( SIDEffects::userEqFreq[ 0 ] ),
										std::log10 ( SIDEffects::userEqFreq[ 1 ] ),
										std::log10 ( SIDEffects::userEqFreq[ 2 ] ),
										std::log10 ( UI::fft::fftFreqStop ) };
		constexpr float	ctrlX[ 5 ] = { 0.0f, 1.0f / 6.0f, 3.0f / 6.0f, 5.0f / 6.0f, 1.0f };

		for ( auto bin = 1; bin < int ( table.size () ); ++bin )
		{
			const auto	lf = std::log10 ( FFTMeasurement::binToFreq ( bin, FFTMeasurement::FFT_SIZE ) );

			auto	seg = 0;
			while ( seg < 3 && lf > ctrlLog[ seg + 1 ] )
				++seg;

			const auto	t = ( lf - ctrlLog[ seg ] ) / ( ctrlLog[ seg + 1 ] - ctrlLog[ seg ] );
			table[ bin ] = std::clamp ( ctrlX[ seg ] + t * ( ctrlX[ seg + 1 ] - ctrlX[ seg ] ), 0.0f, 1.0f );
		}

		return table;
	} ();

	const auto	w = float ( getWidth () );
	const auto	h = float ( getHeight () );
	const auto	centerY = h * 0.5f;

	const auto	slotW = w / float ( numBars );
	const auto	barW = std::max ( 1.0f, slotW - barGap );

	// Each bar keeps the loudest bin of its log-frequency slice
	const auto	collect = [ & ] ( const FFTMeasurement& m, float* band )
	{
		std::fill_n ( band, numBars, 0.0f );

		const auto	levels = m.levels ();

		for ( auto i = 1; i < int ( levels.size () ); ++i )
		{
			const auto	slot = std::clamp ( int ( normX[ i ] * float ( numBars ) ), 0, numBars - 1 );
			band[ slot ] = std::max ( band[ slot ], levels[ i ] );
		}
	};

	float	bandUp[ maxBars ], bandDown[ maxBars ];

	collect ( *left, bandUp );

	if ( stereo && right )
		collect ( *right, bandDown );
	else
		std::copy_n ( bandUp, numBars, bandDown );

	// One pill per slot, left channel up from the center, right down; at full
	// rest it collapses into a small circle
	juce::Path	bars;

	const auto	minHalf = barW * 0.5f;

	for ( auto k = 0; k < numBars; ++k )
	{
		const auto	up = std::max ( bandUp[ k ] / 80.0f * centerY, minHalf );
		const auto	down = std::max ( bandDown[ k ] / 80.0f * centerY, minHalf );

		const auto	x = float ( k ) * slotW + ( slotW - barW ) * 0.5f;

		bars.addRoundedRectangle ( juce::Rectangle<float> ( x, centerY - up, barW, up + down ), minHalf );
	}

	g.setGradientFill ( gradient );
	g.fillPath ( bars );
}
//-----------------------------------------------------------------------------

GUI_EQCurve::Curve::Curve ()
{
	setInterceptsMouseClicks ( false, true );

	bubble.setSize ( 50, 22 );
	bubble.setFont ( UI::font ( UI::fonts::eq_label ) );

	for ( auto band = 0; band < 3; ++band )
	{
		handles[ band ] = std::make_unique<Handle> ( *this, band );
		addAndMakeVisible ( *handles[ band ] );
	}
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::restorePreferences ()
{
	for ( auto band = 0; band < 3; ++band )
		bandOffset[ band ] = preferences->get<float> ( offsetKey ( band ) );

	rebuildCurve ();
	layoutHandles ();
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::paint ( juce::Graphics& g )
{
	// Line and dots each sit on a slightly larger background-colored twin
	const auto	background = UI::getShade ( boxBlend );
	const auto	haloW = UI::lineWidth ( UI::lines::eq_halo );
	const auto	withHalo = UI::lines::visible ( haloW );

	if ( const auto lineW = UI::lineWidth ( UI::lines::eq_curve ); UI::lines::visible ( lineW ) )
	{
		juce::Path	line;

		if ( withHalo )
		{
			juce::PathStrokeType ( lineW + haloW * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded ).createStrokedPath ( line, intentCurve );
			g.setColour ( background );
			g.fillPath ( line );
		}

		juce::PathStrokeType ( lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded ).createStrokedPath ( line, intentCurve );
		g.setGradientFill ( gradient );
		g.fillPath ( line );
	}

	if ( const auto dotW = UI::lineWidth ( UI::lines::eq_dot ); UI::lines::visible ( dotW ) )
	{
		// A hovered dot grows a bit
		juce::Path	halos, dots;

		for ( const auto& h : handles )
			if ( h->isVisible () )
			{
				const auto	size = dotW + ( h->isMouseOverOrDragging () ? hoverGrow : 0.0f );
				const auto	dot = h->getBounds ().toFloat ().withSizeKeepingCentre ( size, size );

				if ( withHalo )
					halos.addEllipse ( dot.expanded ( haloW ) );

				dots.addEllipse ( dot );
			}

		if ( withHalo )
		{
			g.setColour ( background );
			g.fillPath ( halos );
		}

		g.setGradientFill ( gradient );
		g.fillPath ( dots );
	}
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::resized ()
{
	gradient = temperatureGradient ( UI::paddingDef ( UI::paddings::eq_curve_alpha ).top );

	rebuildCurve ();
	layoutHandles ();
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::lookAndFeelChanged ()
{
	gradient = temperatureGradient ( UI::paddingDef ( UI::paddings::eq_curve_alpha ).top );
	bubble.setFont ( UI::font ( UI::fonts::eq_label ) );
}
//-----------------------------------------------------------------------------

// Deflection reads as temperature: boost warm, cut cool, neutral green at rest
juce::ColourGradient GUI_EQCurve::Curve::temperatureGradient ( const float alpha ) const
{
	juce::ColourGradient	anchors ( findColour ( UI::colors::eq_hot ), 0.0f, offsetToY ( 1.0f ),
									  findColour ( UI::colors::eq_cold ), 0.0f, offsetToY ( -1.0f ), false );
	anchors.addColour ( 0.5, findColour ( UI::colors::eq_neutral ) );

	auto	bridged = GUI_AppLookAndFeel::withOklchStops ( anchors );
	bridged.multiplyOpacity ( alpha );

	return bridged;
}
//-----------------------------------------------------------------------------

// One global offset per band (-1..+1, scaled to dB in SIDEffects): the user's
// listening-environment adaptation, applied on top of every mode's preset
// curve
juce::String GUI_EQCurve::Curve::offsetKey ( const int band )
{
	return juce::String ( "eq/" ) + bandNames[ band ];
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::handleDragged ( const int band, const juce::Point<float> pos )
{
	bandOffset[ band ] = std::clamp ( yToOffset ( pos.y ), -1.0f, 1.0f );

	publishBand ( band );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::resetOffset ( const int band )
{
	bandOffset[ band ] = 0.0f;

	publishBand ( band );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::showBubble ( const int band )
{
	// Hosted by the settings box around the widget, so it can float above the
	// handle even at full boost
	const auto	host = getParentComponent () != nullptr ? getParentComponent ()->getParentComponent () : nullptr;
	if ( host == nullptr )
		return;

	bubble.setText ( offsetText ( band ) );

	const auto	handleTop = juce::Point<float> ( bandToX ( band, float ( getWidth () ) ),
												 offsetToY ( bandOffset[ band ] ) - handleSize () * 0.5f );
	const auto	inHost = host->getLocalPoint ( this, handleTop );

	bubble.setTopLeftPosition ( int ( inHost.x ) - bubble.getWidth () / 2,
								int ( inHost.y ) - bubble.getHeight () - 2 );

	if ( ! bubble.isVisible () )
	{
		host->addChildComponent ( bubble );
		juce::Desktop::getInstance ().getAnimator ().fadeIn ( &bubble, 200 );
	}
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::hideBubble ()
{
	if ( ! bubble.isVisible () )
		return;

	juce::Desktop::getInstance ().getAnimator ().fadeOut ( &bubble, 200 );

	if ( auto host = bubble.getParentComponent () )
		host->removeChildComponent ( &bubble );

	bubble.setVisible ( false );
}
//-----------------------------------------------------------------------------

juce::String GUI_EQCurve::Curve::offsetText ( const int band ) const
{
	const auto	pattern = juce::SharedResourcePointer<Strings> ()->get ( "settings/eq/handle" ).toStdString ();

	return fmt::format ( fmt::runtime ( pattern ), bandOffset[ band ] * SIDEffects::userEqOffsetRange );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::publishBand ( const int band )
{
	preferences->set ( offsetKey ( band ), bandOffset[ band ] );
	msg::SettingChanged { "eq", {} }.send ();

	rebuildCurve ();
	layoutHandles ();
	repaint ();

	// Both publishers are handle interactions, so the bubble is in play
	showBubble ( band );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::layoutHandles ()
{
	const auto	hit = juce::roundToInt ( handleSize () );

	for ( auto band = 0; band < 3; ++band )
	{
		handles[ band ]->setSize ( hit, hit );
		handles[ band ]->setCentrePosition ( int ( bandToX ( band, float ( getWidth () ) ) ), int ( offsetToY ( bandOffset[ band ] ) ) );
	}
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::rebuildCurve ()
{
	intentCurve.clear ();

	if ( getWidth () <= 0 )
		return;

	// The intent line runs flat off both shelves and eases through the
	// handles with horizontal tangents
	{
		const float	x[ 3 ] = { bandToX ( 0, float ( getWidth () ) ), bandToX ( 1, float ( getWidth () ) ), bandToX ( 2, float ( getWidth () ) ) };
		const float	y[ 3 ] = { offsetToY ( bandOffset[ 0 ] ), offsetToY ( bandOffset[ 1 ] ), offsetToY ( bandOffset[ 2 ] ) };

		intentCurve.startNewSubPath ( 0.0f, y[ 0 ] );
		intentCurve.lineTo ( x[ 0 ], y[ 0 ] );

		for ( auto i = 1; i < 3; ++i )
		{
			const auto	midX = ( x[ i - 1 ] + x[ i ] ) * 0.5f;
			intentCurve.cubicTo ( midX, y[ i - 1 ], midX, y[ i ], x[ i ], y[ i ] );
		}

		intentCurve.lineTo ( float ( getWidth () ), y[ 2 ] );
	}
}
//-----------------------------------------------------------------------------

// Offset 0 sits at the vertical center; handleMargin keeps the handles fully
// inside the component at full deflection

float GUI_EQCurve::Curve::halfTravel () const
{
	return std::max ( 1.0f, float ( getHeight () ) * 0.5f - handleMargin () );
}
//-----------------------------------------------------------------------------

float GUI_EQCurve::Curve::offsetToY ( const float offset ) const
{
	return float ( getHeight () ) * 0.5f - offset * halfTravel ();
}
//-----------------------------------------------------------------------------

float GUI_EQCurve::Curve::yToOffset ( const float y ) const
{
	return ( float ( getHeight () ) * 0.5f - y ) / halfTravel ();
}
//-----------------------------------------------------------------------------

GUI_EQCurve::Curve::Handle::Handle ( Curve& _owner, const int bandIndex )
	: owner ( _owner ), band ( bandIndex )
{
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::Handle::mouseDown ( const juce::MouseEvent& e )
{
	if ( e.mods.isAltDown () )
		owner.resetOffset ( band );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::Handle::mouseDrag ( const juce::MouseEvent& e )
{
	if ( e.mods.isAltDown () )
		return;

	owner.handleDragged ( band, e.getEventRelativeTo ( &owner ).position );
}
//-----------------------------------------------------------------------------

void GUI_EQCurve::Curve::Handle::mouseEnter ( const juce::MouseEvent& )
{
	owner.showBubble ( band );
	owner.repaint ();
}
//-----------------------------------------------------------------------------

// During a drag JUCE holds the exit until the release, so leaving the hit box
// mid-drag keeps the bubble alive
void GUI_EQCurve::Curve::Handle::mouseExit ( const juce::MouseEvent& )
{
	owner.hideBubble ();
	owner.repaint ();
}
//-----------------------------------------------------------------------------
