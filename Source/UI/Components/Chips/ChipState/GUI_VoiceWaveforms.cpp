#include <JuceHeader.h>

#include "GUI_VoiceWaveforms.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

#include "GUI_Voice.h"

//-----------------------------------------------------------------------------

GUI_VoiceWaveforms::GUI_VoiceWaveforms ( const int _shape )
	: shape ( _shape )
{
	static const juce::String	shapeNames[] = {
		"tri", "saw", "pls", "nse",
	};

	setName ( shapeNames[ _shape ] );
}
//-----------------------------------------------------------------------------

void GUI_VoiceWaveforms::paint ( juce::Graphics& g )
{
	if ( ! on )
		return;

	constexpr auto	cycleCnt = 9.0f;

	path.clear ();

	switch ( shape )
	{
		case 0:	// Triangle
			path.startNewSubPath ( { 0.0f, 0.0f } );

			for ( auto cycle = 1.0f; cycle <= cycleCnt; cycle += 1.0f )
			{
				path.lineTo ( { cycle - 0.5f, 1.0f } );
				path.lineTo ( { cycle, 0.0f } );
			}
			break;

		case 1:	// Sawtooth
			path.startNewSubPath ( { 0.0f, 0.0f } );

			for ( auto cycle = 1.0f; cycle <= cycleCnt; cycle += 1.0f )
			{
				path.lineTo ( { cycle, 1.0f } );
				path.lineTo ( { cycle, 0.0f } );
			}
			break;

		case 2:	// Pulse
		{
			const auto	pos = std::clamp ( float ( curPW ) / float ( 0xFFF ), 0.001f, 0.999f );

			path.startNewSubPath ( { 0.0f, 0.0f } );
			path.lineTo ( { pos, 0.0f } );
			path.lineTo ( { pos, 1.0f } );
			path.lineTo ( { 1.0f, 1.0f } );

			for ( auto cycle = 1.0f; cycle <= cycleCnt; cycle += 1.0f )
			{
				path.lineTo ( { cycle, 0.0f } );
				path.lineTo ( { cycle + pos, 0.0f } );
				path.lineTo ( { cycle + pos, 1.0f } );
				path.lineTo ( { cycle + 1.0f, 1.0f } );
			}
		}
		break;

		case 3:	// Noise
			{
				constexpr auto	cycleSpd = 1.0f / 2.0f;

				juce::Random	curNse ( curIndex * 0xDEADBEEF );

				auto	lastRnd = curNse.nextFloat ();

				path.startNewSubPath ( { 0.0f, lastRnd } );
				path.lineTo ( { cycleSpd, lastRnd } );

				for ( auto cycle = cycleSpd; cycle <= cycleCnt; cycle += cycleSpd )
				{
					lastRnd = curNse.nextFloat ();

					path.lineTo ( { cycle, lastRnd } );
					path.lineTo ( { cycle + cycleSpd, lastRnd } );
				}
			}
			break;
	}

	const auto	b = UI::padded ( getLocalBounds ().toFloat (), UI::paddings::chip_waveform );
	const auto	pitchScale = std::clamp ( curNote / 12.0f, 1.5f, cycleCnt - 1.0f );

	const auto	transform = juce::AffineTransform::translation ( -( cycleCnt / 2.0f ), 0.0f )
							.scaled ( b.getWidth () / pitchScale, -b.getHeight ())
							.translated ( b.getCentreX (), b.getBottom () );

	g.setColour ( findColour ( UI::colors::voiceOn, true ) );
	g.strokePath ( path, juce::PathStrokeType ( UI::lineWidth ( UI::lines::chip_states ), juce::PathStrokeType::JointStyle::beveled ), transform );
}
//-----------------------------------------------------------------------------

void GUI_VoiceWaveforms::setState ( const bool enabled, const uint16_t pitch, const float note, const uint16_t pw, const int index )
{
	const auto	changed = on != enabled || pitch != curPitch || pw != curPW || index != curIndex;

	if ( ! changed )
		return;

	on = enabled;
	curPitch = pitch;
	curNote = note;
	curPW = pw;
	curIndex = index;

	repaint ();
}
//-----------------------------------------------------------------------------
