#pragma once

#include <JuceHeader.h>

#include <optional>

#include "libSidplayEZ/src/EZ/chip-profile-selector.h"
#include "libSidplayEZ/src/EZ/dsp-subsonic-filter.h"
#include "libSidplayEZ/src/EZ/player.h"

#include "Audio/PerceivedLoudness.h"

#include "Effects/FX_Helpers.h"
#include "sid-constants.h"
#include "SIDEffects.h"

struct SidTuneInfoEZ;

class SIDPlayer final : private juce::Thread
{
public:
	SIDPlayer ();
	~SIDPlayer () override;

	void setSamplerate ( const int _sampleRate )	{	engineEZ.setSamplerate ( _sampleRate );	}
	void setOutputLatency ( const int _latency )	{	outputLatency = _latency;				}

	// The replay-gain target every rated tune is leveled to
	static constexpr auto	targetLUFS = -18.0f;

	[[ nodiscard ]] bool load ( const char* filename );
	[[ nodiscard ]] bool load ( SidTune::LoaderFunc loader, const char* filename );

	// Loads from a real file (absolute path) or from factory data (data-relative)
	[[ nodiscard ]] bool loadTune ( const std::string& name );

	bool init ( const unsigned int songNo, const bool useFilter );
	[[ nodiscard ]] bool play ( float* const* dst, int lengthWanted );
	bool startRender ( uint32_t lengthMS, uint32_t fadeOutLenMS, float ebuLevel, uint32_t skipMS = 0 );

	// Renders synchronously on the calling thread (the export pipeline);
	// the abort hook is polled once per 60Hz chunk. An aborted render can be
	// RESUMED by calling again (state is kept), only init () starts over
	[[ nodiscard ]] bool renderBlocking ( uint32_t lengthMS, uint32_t fadeOutLenMS, float ebuLevel, uint32_t skipMS, const std::function<bool ()>& shouldAbort );

	// Export helpers, hand the finished render over to the caller
	[[ nodiscard ]] juce::AudioBuffer<float> takeWaveform ();
	[[ nodiscard ]] float getMeasuredGain () const	{	return ebuGainMain;	}

	// Fired on the render thread when a live loudness measurement completes,
	// with the two raw measurements ( integrated, midband ) in LUFS
	std::function<void ( float integratedLUFS, float midLUFS )>	onLoudnessMeasured;

	[[ nodiscard ]] uint32_t getRenderLength () const	{ return renderLengthMs;	}

	void stopRender ();
	void togglePlayPause ();
	void seek ( uint32_t positionMS );

	[[ nodiscard ]] bool isReadyToPlay () const		{	return readyToPlay;						}
	[[ nodiscard ]] bool isPaused () const			{	return readyToPlay ? paused.load () : true;	}
	[[ nodiscard ]] int getNumChips () const		{	return engineEZ.getNumChips ();			}

	[[ nodiscard ]] const SidTuneInfoEZ& getFileInfo () const	{	return engineEZ.getFileInfo (); }
	[[ nodiscard ]] const SidTune& getSidTune () const			{	return engineEZ.getSidTune ();	}

	[[ nodiscard ]] unsigned int getCurrentSong () const	{	return engineEZ.getFileInfo ().currentSong;		}
	[[ nodiscard ]] unsigned int getNumberOfSongs () const	{	return engineEZ.getFileInfo ().numSongs;		}

	[[ nodiscard ]] uint32_t getTimeMS () const		{	return uint32_t ( renderPlayOffset / 44.1f );	}
	[[ nodiscard ]] uint32_t getRenderProgressMS () const	{	return engineEZ.getEmulatedTimeMs ();	}

	[[ nodiscard ]] bool finishedPlaying () const;

	[[ nodiscard ]] std::pair<uint8_t*, int> getSidStatus ( int sidNum ) const;
	[[ nodiscard ]] uint16_t getCPUCycles () const;
	[[ nodiscard ]] bool lockDigiBuffers ();
	void unlockDigiBuffers ();
	// The display window inside the rolling digi waveform, plus how many
	// valid samples the buffer holds before it (the display's lock history)
	[[ nodiscard ]] std::pair<int8_t*, int> getDigiBuffer ( int sidNum );

	[[ nodiscard ]] int getNumChannels () const	{	return waveform.getNumChannels ();	}

	[[ nodiscard ]] bool isNTSC () const	{	return engineEZ.getFileInfo ().clock == "NTSC";	}

	void setSharedConfig ( std::shared_ptr<const libsidplayEZ::SharedPlayerConfig> config ) { engineEZ.setSharedConfig ( std::move ( config ) ); }
	[[ nodiscard ]] const libsidplayEZ::OverrideSelector::overrideMap& getAllTuneOverrides () const { return engineEZ.getAllTuneOverrides (); }

	void setRoms ( const void* kernal, const void* basic = nullptr, const void* character = nullptr )	{	engineEZ.setRoms ( kernal, basic, character );	}

	void setDacLeakage ( const double leakage ) {	engineEZ.setDacLeakage ( leakage );		}
	void setVoiceDrift ( const double drift )	{	engineEZ.set6581VoiceDrift ( drift );	}

	void setReplayGain ( const bool use ) { useReplayGain = use; }

	//
	// Live-tweak mode (chip-profile editor): the render thread stays only
	// ~100 ms ahead of the playhead, so pushed profile changes become audible
	// almost immediately instead of landing in already-rendered audio
	//
	using ChipSettings = libsidplayEZ::ChipProfileSelector::settings;

	void setLiveTweak ( const bool enabled )	{	liveTweak = enabled;	}
	[[ nodiscard ]] bool isLiveTweak () const	{	return liveTweak;		}

	// Message thread -> render thread; applied between emulation chunks
	void pushLiveProfile ( const ChipSettings& s );

private:
	// Set from the message thread, read by the audio callback and the render thread
	std::atomic<bool>	useReplayGain = true;
	std::atomic<bool>	readyToPlay = false;
	std::atomic<bool>	paused = false;
	std::atomic<int>	lenLeft = 0;	// Written by the audio callback, read by finishedPlaying ()
	int		outputLatency = 0;

	libsidplayEZ::Player	engineEZ;

	// juce::Thread
	uint32_t	renderLengthMs = 0;
	uint32_t	fadeOutLengthMS = 0;

	// Silent-intro length pre-rendered into scratch buffers on a fresh render
	uint32_t	skipStartMS = 0;
	void run () override;

	// The render loop shared by startRender (own thread) and renderBlocking
	bool renderCore ( const std::function<bool ()>& shouldAbort );

	// this
	static constexpr auto	sixtyHzLength = 44100u / 60u;

	// Live-tweak: how far the render may run ahead of the playhead
	static constexpr auto	liveAheadSamples = 4410;	// 100 ms

	std::atomic<bool>	liveTweak = false;
	std::atomic<bool>	liveProfileDirty = false;
	juce::SpinLock		liveProfileLock;
	ChipSettings		liveProfile;					// guarded by liveProfileLock

	// Render-thread only; empty forces a full apply (after init () reset the
	// engine to CSV values)
	std::optional<ChipSettings>	lastAppliedProfile;

	void applyLiveProfile ();							// render thread, between chunks

	std::atomic<bool>	rendered = false;
	bool	renderStarted = false;		// Buffers allocated; a resumed render skips that
	bool	faded = false;

	// Play cursor: advanced by the audio callback, moved by seek () from the
	// message thread, play () works on one snapshot per callback
	std::atomic<int>	renderPlayOffset = 0;

	// Render cursor: published by the render thread with release ordering
	// after the sample stores, read by the audio callback with acquire,
	// seeing the new progress guarantees seeing the samples behind it
	std::atomic<int>	renderProgress = 0;

	PerceivedLoudness	ebu { 44100.0, 2 };	// Member: loudness accumulates across a paused+resumed render

	[[ nodiscard ]] int getDataIndex ( const int dataSize ) const;

	float				ebuLevel = -96.0f;
	std::atomic<float>	ebuGainMain = 1.0f;
	SmoothedValue		ebuGain[ 2 ] = { 1.0f, 1.0f };

	juce::CriticalSection		constructionLock;

	juce::CriticalSection		waveformLock;
	juce::AudioBuffer<float>	waveform;

	libsidplayEZ::dsp::SubsonicFilter	subsonicFilter[ 2 ];

	template <typename T>
	requires std::is_trivially_constructible_v<T>
	struct uninitialized
	{
		T val;

		uninitialized () {}
		uninitialized ( T v ) : val ( v ) {}

		operator T ()                        const { return val; }
		uninitialized<T>& operator= ( T v ) { val = v; return *this; }

		[[ nodiscard ]] static T* raw ( std::vector<uninitialized<T>>& v )
		{
			return reinterpret_cast<T*>( v.data () );
		}
	};

	// One entry per chip the current tune uses
	std::vector<std::vector<uninitialized<int8_t>>>	digiWaveforms;

	using regs = std::vector<std::array<uninitialized<uint8_t>, 32>>;
	std::vector<regs>	registers;

	std::vector<uninitialized<uint16_t>>	cycles;
};
//-----------------------------------------------------------------------------
