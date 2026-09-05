#include <cmath>

#include "GUI_ChipFrequencyLines.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "Audio/sid-constants.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_ChipFrequencyLines::GUI_ChipFrequencyLines ()
{
	reset ( false );
}
//-----------------------------------------------------------------------------

void GUI_ChipFrequencyLines::paint ( juce::Graphics& g )
{
	const auto	voiceHeight = ( getHeight () - UI::fft::glow * 2.0f ) / SID::numVoices;

	const auto		lineWidth = UI::lineWidth ( UI::lines::chip_voice_frequency );
	constexpr auto	decay = 1.0f / UI::fft::numHistory;

	if ( ! UI::lines::visible ( lineWidth ) )
		return;

	// (re)create particle images
	{
		const auto	scale = g.getInternalContext ().getPhysicalPixelScaleFactor ();

		const auto	newParticleWidth = int ( std::ceil ( ( lineWidth + UI::fft::glow * 2.0f ) * scale ) );
		const auto	newParticleHeight = int ( std::ceil ( ( voiceHeight + UI::fft::glow * 2.0f ) * scale ) );

		if ( particleWidth != newParticleWidth || particleHeight != newParticleHeight
			 || ! juce::approximatelyEqual ( particleLineWidth, lineWidth ) )
		{
			particleWidth = newParticleWidth;
			particleHeight = newParticleHeight;
			particleLineWidth = lineWidth;

			for ( auto colIdx = 0; auto& img : particles )
			{
				img = juce::Image ( juce::Image::ARGB, particleWidth, particleHeight, true );

				auto drawBlock = [ this, colIdx, voiceHeight, scale, lineWidth, &img ] ( const float alpha, const float reducedY, const float toWhite )
				{
					juce::Graphics	imgG ( img );

					imgG.setColour ( voiceColorTable[ colIdx ].interpolatedWith ( juce::Colours::white, toWhite ).withMultipliedAlpha ( alpha ) );

					auto	r = juce::Rectangle<float> { UI::fft::glow * scale, UI::fft::glow * scale, lineWidth * scale, voiceHeight * scale }.reduced ( 0.0f, reducedY * scale );

					if ( toWhite > 0.0f )
						r = r.reduced ( 0.75f * scale );

					imgG.fillRoundedRectangle ( r, r.getWidth () * 0.5f );
				};

				// Glow
				drawBlock ( decay * 2.0f, 0.0f, 0.0f );
				gin::applyStackBlur ( img, int ( UI::fft::glow ) );

				// Block itself
				drawBlock ( 1.0f, 0.5f, 0.0f );

				// Thin, white line
				drawBlock ( 1.0f, 0.5f, 0.5f );

				++colIdx;
			}
		}
	}

	for ( auto alpha = decay; const auto& voices : lines )
	{
		auto	b = getLocalBounds ().toFloat ().reduced ( 0.0f, UI::fft::glow );

		for ( const auto& v : voices )
		{
			const auto	r = b.removeFromTop ( voiceHeight );

			const auto	freq = v.freq;

			if ( freq < 0.0f )
				continue;

			const auto	newAlpha = UI::fft::pow2 ( alpha * v.volume );
			if ( newAlpha < 0.01f )
				continue;

			g.setOpacity ( newAlpha );
			g.drawImage ( particles[ v.colIdx ], r.withX ( freq * r.getWidth () ).withWidth ( 0.0f ).expanded ( UI::fft::glow + lineWidth * 0.5f, UI::fft::glow ) );
		}

		alpha += decay;
	}
}
//-----------------------------------------------------------------------------

// Chips are created lazily, long after the theme's sendLookAndFeelChange
// broadcast swept the tree; fetch the colors once a LookAndFeel is reachable
void GUI_ChipFrequencyLines::parentHierarchyChanged ()
{
	colourChanged ();
}
//-----------------------------------------------------------------------------

void GUI_ChipFrequencyLines::colourChanged ()
{
	voiceColorTable[ 0 ] = findColour ( UI::colors::voiceOn );			// not filtered
	voiceColorTable[ 1 ] = findColour ( UI::colors::filterOn );			// filtered
	voiceColorTable[ 2 ] = findColour ( UI::colors::voiceMuted );		// muted
	voiceColorTable[ 3 ] = findColour ( UI::colors::filterOn );			// muted, but filtered = audible

	// The particle images bake these colors, force a re-render
	particleWidth = 0;
	particleHeight = 0;

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ChipFrequencyLines::reset ( const bool isNTSC )
{
	clockspeed = isNTSC ? SID::NTSC_CLOCK : SID::PAL_CLOCK;

	for ( auto& l : lines )
		std::ranges::fill ( l, freqLine {} );

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ChipFrequencyLines::updateState ( uint8_t* regs, const int regIndex )
{
	auto	count = std::min ( regIndex, UI::fft::numHistory );
	if ( ! count )
		return;

	auto pow3 = [] ( const float a )
	{
		return a * a * a;
	};

	while ( count-- )
	{
		auto	voiceVolOffset = 0x1d;

		const auto	filterMode = uint8_t ( ( regs[ 0x18 ] >> 4 ) & 0x7 );
		auto		routing = uint8_t ( regs[ 0x17 ] & 7 );
		auto		muted = uint8_t ( ( regs[ 0x18 ] & 0x80 ) >> 4 );

		for ( auto registerOffset = 0; auto& v : lines[ count ] )
		{
			const auto	filtered = ( routing & 1 ) && filterMode;
			const auto	colIdx = int ( filtered ) + ( muted & 2 );

			const auto	pitch = uint16_t ( ( regs[ registerOffset + 0x01 ] << 8 ) + regs[ registerOffset + 0x00 ] );
			const auto	freq = pitchRegToFreq ( pitch );
			const auto	norm = UI::fft::freqToNormalized ( freq );

			v.freq = norm >= 0.0f && norm < 1.0f ? norm : -1.0f;
			v.volume = 1.0f - pow3 ( 1.0f - regs[ voiceVolOffset ] * ( 1.0f / 255.0f ) );
			v.colIdx = colIdx;

			registerOffset += SID::REGISTER_VOICE_DELTA;
			++voiceVolOffset;

			routing >>= 1;
			muted >>= 1;
		}

		regs -= 32;
	}

	repaint ();
}
//-----------------------------------------------------------------------------
