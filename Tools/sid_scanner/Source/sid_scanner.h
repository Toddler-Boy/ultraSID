#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>

#include "libSidplayEZ/src/EZ/player.h"

#include "Config/HVSCSource.h"
#include "Config/TunePatches.h"

//-----------------------------------------------------------------------------

// A copied exe on a machine without the repo checkout runs portable:
// everything lives in one Data folder next to the exe
inline bool isPortable ()
{
	static const auto portable = ! juce::File ( SID_TOOLS_ROOT ).isDirectory ();
	return portable;
}
//-----------------------------------------------------------------------------

// ultraSID's Data folder: the checkout's in development, the exe sibling
// when portable
inline juce::File dataRoot ()
{
	return isPortable ()
		? juce::File::getSpecialLocation ( juce::File::SpecialLocationType::currentExecutableFile ).getParentDirectory ().getChildFile ( "Data" )
		: juce::File ( SID_TOOLS_ROOT ).getChildFile ( "Data" );
}
//-----------------------------------------------------------------------------

// The scanner's own files (SID_*.txt, bugs.txt), independent of the working
// directory: Tools/sid_scanner/Data in development, the shared Data folder
// when portable
inline juce::File scannerDataFile ( const juce::String& filename )
{
	const auto	folder = isPortable () ? dataRoot ()
										 : juce::File ( SID_TOOLS_ROOT ).getChildFile ( "Tools/sid_scanner/Data" );

	folder.createDirectory ();

	return folder.getChildFile ( filename );
}
//-----------------------------------------------------------------------------

// Songdelays.md5 ships as-is, so the scanner works directly on the shipping
// copy in Databases (the repo file in development)
inline juce::File songdelaysFile ()
{
	return dataRoot ().getChildFile ( "Databases/Songdelays.md5" );
}
//-----------------------------------------------------------------------------

// The HVSC location from ultraSID's own settings (read-only load, same
// HVSC_BASE / Documents fallback as ultraSID itself)
[[ nodiscard ]] juce::File ultraSIDHVSCPath ();
//-----------------------------------------------------------------------------

// Resolves a tune key ("/MUSICIANS/...") against the attached hvscsource: the
// collection-relative path (leading slash kept, what the profile selectors
// match on), the Exotic-tunes mirror's absolute path, or "" when nowhere
[[ nodiscard ]] inline juce::String resolveTuneSpec ( const juce::String& name )
{
	if ( hvscsource::exists ( name + ".sid" ) )
		return name + ".sid";

	if ( auto mirrored = dataRoot ().getChildFile ( "Exotic tunes" + name + ".sid" ); mirrored.existsAsFile () )
		return mirrored.getFullPathName ();

	return {};
}
//-----------------------------------------------------------------------------

// SidTune::LoaderFunc over both resolveTuneSpec forms
void scannerLoadBytes ( const char* fileName, std::vector<uint8_t>& bufferRef );
//-----------------------------------------------------------------------------

class MeasureLoudness
{
public:
	explicit MeasureLoudness ( std::shared_ptr<const libsidplayEZ::SharedPlayerConfig> config );

	// Feature-flag bits published into measureTune's featureFlags output. Bits only
	// ever turn on: filter, digi and delayed-start latch as soon as they are
	// detected mid-render, one-shot is only known once the loop check has run
	enum FeatureFlags : uint8_t
	{
		featFilter			= 1 << 0,
		featDigi			= 1 << 1,
		featOneShot			= 1 << 2,
		featDelayedStart	= 1 << 3,
	};

	struct result
	{
		std::string	error;
		float		loudness = -96.0f;
		float		midLoudness = -96.0f;	// loudness of the midband-filtered signal, stored beside the loudness
		bool		filterUsed = true;
		bool		digiUsed = true;
		bool		looped = true;
		uint32_t	startMs = 0;

		// The CPU jammed this far into an otherwise valid render (0 = it didn't):
		// the measurement stands, but the rip deserves a bugs.txt entry
		uint32_t	jammedAtMs = 0;

		// A filter-less render hit filter routing: every value above is invalid,
		// the tune needs a fresh render with useFilter on
		bool		filterMismatch = false;

		// The settings fingerprint computed at load, stored with the
		// measurements; settingsMatch = it equals the stored one, the existing
		// measurements are still valid and nothing was rendered
		std::string	settingsHash;
		bool		settingsMatch = false;

		// Unknown-mode scans only: registers written at digi rates, as
		// "reg:maxPerBlock/busyBlocks" pairs; "-" = scanned clean, empty =
		// not an unknown-mode scan
		std::string	writeRates;

		// The capture-mode suggestion derived from which registers race;
		// empty = nothing beyond the auto-detected $D418
		std::string	digiHint;
	};

	// storedSettingsHash, when non-empty, is compared against the settings
	// fingerprint computed after load; a match returns settingsMatch without
	// rendering (the caller only passes it when valid measurements exist).
	// renderedMs, when given, is continuously updated with the number of milliseconds
	// rendered so far, so a GUI thread can display per-tune progress.
	// speedSample, when given, is updated every 600 blocks (10s of audio) and once at
	// the end with a consistent pair of (rendered audio ms << 32 | render wall ms),
	// measured over the render loop only, the basis for the rendering-speed display.
	// featureFlags, when given, has FeatureFlags bits or-ed in as detections happen.
	// abortFlag, when given, is polled once per render block; raising it makes the
	// measurement bail out quickly, returning the error "aborted"
	result measureTune ( const char* filename, const int tuneNo, const uint32_t lengthMS, const bool useFilter, const std::string& storedSettingsHash = {}, const bool force6581 = false, const bool force8580 = false, std::atomic<uint32_t>* renderedMs = nullptr, std::atomic<uint64_t>* speedSample = nullptr, std::atomic<uint8_t>* featureFlags = nullptr, const std::atomic<bool>* abortFlag = nullptr );

private:
	libsidplayEZ::Player	engine;

	float	outBufferL[ 1024 ];
	float	outBufferR[ 1024 ];
};
//-----------------------------------------------------------------------------
