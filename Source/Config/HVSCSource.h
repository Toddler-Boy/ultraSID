#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <vector>

class ZipFolder;

//-----------------------------------------------------------------------------

// The HVSC collection behind one API: a plain C64Music folder, or a single
// zip archive holding it. setRoot decides the mode from what the path names;
// every path here is collection-relative ("DOCUMENTS/HVSC.txt"). Set the
// root only while nothing reads

namespace hvscsource
{
	// Accepts the C64Music folder or a zip of it (entries at the archive root
	// or under a single "C64Music/" folder). False when neither is usable
	bool setRoot ( const juce::File& rootOrZip );

	[[ nodiscard ]] bool isZipMode ();

	// An existing file ending in a zip central directory
	[[ nodiscard ]] bool isZipArchive ( const juce::File& file );

	// The archive behind the facade, nullptr in folder mode; collection paths
	// inside it carry archivePrefix ("C64Music/" when the zip wraps the folder)
	[[ nodiscard ]] ZipFolder* archive ();
	[[ nodiscard ]] juce::String archivePrefix ();

	[[ nodiscard ]] bool exists ( const juce::String& path );
	[[ nodiscard ]] bool folderExists ( const juce::String& path );

	// Whole files; empty on a miss
	[[ nodiscard ]] juce::MemoryBlock loadData ( const juce::String& path );
	[[ nodiscard ]] juce::String loadText ( const juce::String& path );

	// nullptr on a miss. Deflated zip entries stream front-to-back only
	[[ nodiscard ]] std::unique_ptr<juce::InputStream> createStream ( const juce::String& path );

	// Every entry valid under rootOrZip, without touching the active root:
	// trailing '/' = folder, otherwise file
	[[ nodiscard ]] bool allPathsValid ( const juce::File& rootOrZip, const juce::StringArray& arr );

	// SidTune::LoaderFunc-shaped byte loader for collection tunes
	void loadBytes ( const char* fileName, std::vector<uint8_t>& bufferRef );
}
//-----------------------------------------------------------------------------
