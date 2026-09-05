#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <map>
#include <vector>

//-----------------------------------------------------------------------------

// Byte fixes for broken collection rips (Databases/tune-patches.txt): a tune
// line is its collection path, followed by the corrected song lengths when
// the fix changes how long the tune plays; each line below it is "offset old
// new" in hex. The bytes are applied by the tune loaders after the bytes are
// read, before the engine sees them; the lengths by the Songlengths consumers

namespace tunepatches
{
	// Parses the file text; call only while nothing loads tunes
	void load ( const juce::String& text );

	// Applies the tune's patches in place, all or nothing: false when the tune
	// has none or its bytes don't match the expected originals
	bool apply ( const char* collectionPath, std::vector<uint8_t>& bytes );

	// Song-length overrides, keyed by collection path with its leading slash
	// (the Songlengths spelling), the values as written for the Database layer
	// to parse. Unlike the bytes these can't retire themselves, the block is
	// deleted once the HVSC ships the fixed file
	[[ nodiscard ]] const std::map<juce::String, juce::String>& lengths ();
}
//-----------------------------------------------------------------------------
