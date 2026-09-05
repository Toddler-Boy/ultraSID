#include <JuceHeader.h>

#include "GUI_ChipState.h"

#include "std_lime/lime_math.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "chip-constants.h"


//-----------------------------------------------------------------------------

GUI_ChipState::GUI_ChipState ()
	: shadow ( juce::Colours::black.withAlpha ( 0.5f ), 5.0f, { 0.0f, 3.0f } )
{
	setName ( "state" );
	setInterceptsMouseClicks ( false, false );

	reset ();

	for ( auto i = 1; auto& c : voices )
	{
		c.setName ( "voice" + juce::String ( i++ ) );
		addAndMakeVisible ( c );
	}

	// The rows share one box shape (see the shadowPath): outer corners only
	for ( auto i = 0; i < SID::numVoices; ++i )
		voices[ i ].setRoundedEnds ( i == 0, i == SID::numVoices - 1 );

	addAndMakeVisible ( filter );
	addAndMakeVisible ( labels );

	for ( auto c : voices[ 0 ].getChildren () )
		labels.addLabel ( c );

	labels.addLabel ( &filter );
}
//-----------------------------------------------------------------------------

void GUI_ChipState::resized ()
{
	shadowPath.clear ();
}
//-----------------------------------------------------------------------------

void GUI_ChipState::paintOverChildren ( juce::Graphics& g )
{
	if ( shadowPath.isEmpty () )
	{
		const auto	b = voices[ 0 ].getBounds ().withBottom ( voices[ 2 ].getBottom () );

		shadowPath.addRoundedRectangle ( b, UI::corner ( UI::corners::chip_states, b.toFloat () ) );
		shadowPath.addRoundedRectangle ( filter.getBounds (), UI::corner ( UI::corners::chip_states, filter.getBounds ().toFloat () ) );
	}

	shadow.render ( g, shadowPath );
}
//-----------------------------------------------------------------------------

void GUI_ChipState::reset ( const bool ntsc, const std::string& _model )
{
	clockspeed = ntsc ? SID::NTSC_CLOCK : SID::PAL_CLOCK;

	model = _model;

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ChipState::updateState ( uint8_t* regs, const int regIndex )
{
	auto	count = std::min ( regIndex, UI::chip::numHistory );
	if ( ! count )
		return;

	auto	envRegs = regs;

	//
	// Update all voices and the filter, including history
	//
	{
		const auto	filterMode = uint8_t ( ( regs[ 0x18 ] >> 4 ) & 0x7 );

		//
		// Update each voice
		//
		{
			auto		routing = uint8_t ( regs[ 0x17 ] & 7 );
			auto		muted = uint8_t ( regs[ 0x18 ] & 0x80 );

			for ( auto registerOffset = 0; auto& vc : voices )
			{
				const auto	ctrl = regs[ registerOffset + 0x04 ];
				const auto	pitch = uint16_t ( ( regs[ registerOffset + 0x01 ] << 8 ) + regs[ registerOffset + 0x00 ] );
				const auto	pw = uint16_t ( ( ( regs[ registerOffset + 0x03 ] << 8 ) + regs[ registerOffset + 0x02 ] ) & 0xFFF );

				const auto	note = pitchRegToNote ( pitch );

				vc.setState ( regIndex, ctrl, pitch, note, pw, ( routing & 1 ) && filterMode, muted & 0x20 );

				routing >>= 1;
				muted >>= 1;

				registerOffset += SID::REGISTER_VOICE_DELTA;
			}
		}

		//
		// Filter
		//
		{
			const auto	routing = uint8_t ( regs[ 0x17 ] & 7 );

			filter.setState ( filterMode, routing );
		}

		//
		// Loop backwards over history to display pitch/cutoff/resonance curves independent of frame-rate
		//
		while ( count-- )
		{
			// Update each voices' pitch history
			for ( auto registerOffset = 0; auto& vc : voices )
			{
				const auto	pitch = uint16_t ( ( regs[ registerOffset + 0x01 ] << 8 ) + regs[ registerOffset + 0x00 ] );
				const auto	note = pitchRegToNote ( pitch );

				vc.setPitch ( count, note );

				registerOffset += SID::REGISTER_VOICE_DELTA;
			}

			// Update filters' cutoff and resonance history
			{
				const auto	freqReg = uint16_t ( ( regs[ 0x16 ] << 3 ) + ( regs[ 0x15 ] & 0x7 ) );
				const auto	resonance = uint8_t ( regs[ 0x17 ] >> 4 );

				const auto	cutoff = freqRegToNormalized ( lime::remap ( float ( freqReg ) / 2047.0f, 0.0f, 1.0f, minFreq, maxFreq ) );

				filter.addCutoff ( cutoff );
				filter.addResonance ( resonance / 15.0f );
			}
			regs -= 32;
		}

		filter.dataAdded ();

		//
		// Update each voices' envelope history
		//
		count = std::min ( regIndex, UI::chip::numHistory / 2 );
		while ( count-- )
		{
			voices[ 0 ].setEnvelope ( count, envRegs[ 0x1d ] );
			voices[ 1 ].setEnvelope ( count, envRegs[ 0x1e ] );
			voices[ 2 ].setEnvelope ( count, envRegs[ 0x1f ] );

			envRegs -= 32;
		}
	}
}
//-----------------------------------------------------------------------------
