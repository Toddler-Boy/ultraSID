#include "Audio/FXTuning.h"

#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

void GUI_ultraSID::initAudio ()
{
	// The app opens the device itself, so sample rate and block size are respected
	{
		juce::AudioDeviceManager::AudioDeviceSetup	preferred;
		preferred.sampleRate = internalSamplerate;
		preferred.bufferSize = internalSamplerate / 100;	// 10ms

		if ( const auto error = deviceManager.initialise ( 0, 2, nullptr, true, {}, &preferred ); error.isNotEmpty () )
			Z_WARN ( "Audio device init: " << error );
	}

	// Attach audio callback
	setAudioChannels ( 0, 2 );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::disableAudio ()
{
	if ( muted++ )
		return;

	curOutVol.set ( 0.0f );

	auto	maxTries = 5;
	while ( maxTries-- && ! curOutVol.restingAtZero () )
		juce::Thread::sleep ( 5 );

	inAudio.enter ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::enableAudio ()
{
	if ( --muted )
		return;

	curOutVol.set ( 1.0f );

	inAudio.exit ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::prepareToPlay ( int samplesPerBlockExpected, double sampleRate_ )
{
	// The sample rate that the audio device is running at.
	// This is used for resampling from the internal samplerate of 44.1kHz to the output samplerate of the audio device.
	sampleRate = int ( sampleRate_ );

	Z_INFO ( "Audio device running at " << sampleRate << " Hz with " << samplesPerBlockExpected << "-sample blocks" );

	// Tell SID-player to always output at 44.1kHz
	player.setSamplerate ( internalSamplerate );

	// FIFO buffer (for resampling from internal samplerate to output samplerate)
	sidBuffer.setSize ( 2, samplesPerBlockExpected );
	resamplingFifo.setSize ( samplesPerBlockExpected, 2, sampleRate );
	resamplingFifo.setResamplingRatio ( internalSamplerate, double ( sampleRate ) );

	// Get output latency from audio device and set it to player for accurate display timing
	auto	audioDevice = deviceManager.getCurrentAudioDevice ();
	player.setOutputLatency ( audioDevice ? audioDevice->getOutputLatencyInSamples () : 0 );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::releaseResources ()
{
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::getNextAudioBlock ( const juce::AudioSourceChannelInfo& bufferToFill )
{
	if ( bufferToFill.buffer == nullptr )
		return;

	// The whole chain renders stereo, a mono-only device gets silence
	if ( bufferToFill.buffer->getNumChannels () < 2 )
	{
		bufferToFill.clearActiveBufferRegion ();
		return;
	}

	//
	// Blocked: just clear the buffer
	//
	if ( ! inAudio.tryEnter () )
	{
		bufferToFill.clearActiveBufferRegion ();
		return;
	}

	// Everything below works on the region to fill, not the whole buffer
	juce::AudioBuffer<float>	out ( bufferToFill.buffer->getArrayOfWritePointers (),
									  bufferToFill.buffer->getNumChannels (),
									  bufferToFill.startSample,
									  bufferToFill.numSamples );

	//
	// Fill output buffer
	//
	{
		auto renderAudio = [ this ] ( juce::AudioBuffer<float>& buffer )
		{
			auto		buffers = buffer.getArrayOfWritePointers ();
			const auto	numSamples = buffer.getNumSamples ();

			if ( player.play ( buffers, numSamples ) )
			{
				dspEffects.setPlaying ( true );
			}
			else
			{
				dspEffects.setPlaying ( false );

				// The FX still hold a ringing tail: feed silence until it has
				// fully drained, then go idle
				buffer.clear ();

				if ( dspEffects.drained () )
					return;
			}

			dspEffects.process ( buffers, numSamples );

			// Both channels are always collected, so the right displays have
			// current data the moment they become visible
			fftMeasureLeft.pushAudio ( buffers[ 0 ], numSamples );
			fftMeasureRight.pushAudio ( buffers[ 1 ], numSamples );

			dspEffects.applyGlain ( buffers, numSamples );
		};

		if ( sampleRate == internalSamplerate )
		{
			// Render into output directly
			renderAudio ( out );
		}
		else
		{
			while ( resamplingFifo.samplesReady () < out.getNumSamples () )
			{
				renderAudio ( sidBuffer );
				resamplingFifo.pushAudioBuffer ( sidBuffer );
			}

			resamplingFifo.popAudioBuffer ( out );
		}
	}

	//
	// Fade buffer in/out to avoid clicking between songs
	//
	auto isMuting = [ & ]	{	return muted && ! curOutVol.restingAtZero ();	};
	auto isUnmuting = [ & ] {	return ! muted && ! curOutVol.restingAtOne ();	};

	if ( isMuting () || isUnmuting () )
	{
		const auto	numSamples = out.getNumSamples ();
		const auto	buffers = out.getArrayOfWritePointers ();

		for ( auto i = 0; i < numSamples; ++i )
		{
			const auto	vol = curOutVol.getAndStep ();
			buffers[ 0 ][ i ] *= vol;
			buffers[ 1 ][ i ] *= vol;
		}
	}

	inAudio.exit ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updateFX ()
{
	// The user settings
	dspEffects.setFXParameter ( SIDEffects::FXParameter::stereo_processing, preferences->get<bool> ( "fx/stereo-processing" ) ? 1.0f : 0.0f );

	// Mode transition pace (clamped, the range guards hand-edited yml values)
	dspEffects.setFXParameter ( SIDEffects::FXParameter::transition_time,
								std::clamp ( preferences->get<float> ( "fx/transition-time" ), 0.0f, 10.0f ) * 0.01f );

	// The tuning, slider units to 0..1
	const auto&	tuning = fxTuning ();

	auto setValue = [ this ] ( const SIDEffects::FXParameter param, const float value )
	{
		dspEffects.setFXParameter ( param, value * 0.01f );
	};

	// REAL transformer hum (the character is constexpr in FX_TransformerHum)
	setValue ( SIDEffects::FXParameter::real_hum_volume, tuning.humVolume );

	// Splitter
	setValue ( SIDEffects::FXParameter::magic_splitter_freq, tuning.splitterFreq );
	setValue ( SIDEffects::FXParameter::magic_splitter_lowGain, tuning.splitterLowGain );

	// Wide-mono
	setValue ( SIDEffects::FXParameter::magic_wideMono_width, tuning.wideMonoWidth );

	// Delay
	setValue ( SIDEffects::FXParameter::magic_delay_wet, tuning.delayWet );
	setValue ( SIDEffects::FXParameter::magic_delay_feedback, tuning.delayFeedback );

	// Reverb
	setValue ( SIDEffects::FXParameter::magic_reverb_wet, tuning.reverbWet );

	// Noise
	setValue ( SIDEffects::FXParameter::magic_noise_volume, tuning.noiseVolume );
	setValue ( SIDEffects::FXParameter::magic_noise_color, tuning.noiseColor );

	// EPIC
	setValue ( SIDEffects::FXParameter::epic_wideMono_width, tuning.epicWideMonoWidth );
	setValue ( SIDEffects::FXParameter::epic_delay_wet, tuning.epicDelayWet );
	setValue ( SIDEffects::FXParameter::epic_delay_feedback, tuning.epicDelayFeedback );
	setValue ( SIDEffects::FXParameter::epic_reverb_wet, tuning.epicReverbWet );

	// MYTHIC
	setValue ( SIDEffects::FXParameter::mythic_wideMono_width, tuning.mythicWideMonoWidth );
	setValue ( SIDEffects::FXParameter::mythic_delay_wet, tuning.mythicDelayWet );
	setValue ( SIDEffects::FXParameter::mythic_delay_feedback, tuning.mythicDelayFeedback );
	setValue ( SIDEffects::FXParameter::mythic_reverb_wet, tuning.mythicReverbWet );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updateUserEQ ()
{
	static constexpr const char*	bands[] = { "low", "mid", "high" };

	for ( auto band = 0; band < 3; ++band )
		dspEffects.setUserEQOffset ( band, preferences->get<float> ( juce::String ( "eq/" ) + bands[ band ] ) );
}
//-----------------------------------------------------------------------------
