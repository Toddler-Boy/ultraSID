#pragma once

#include <JuceHeader.h>

class ZipFolder;

//-----------------------------------------------------------------------------

namespace Unarchiver
{
	// Caps against hostile archives; defaults are generous for HVSC-sized content
	struct Limits
	{
		uint64_t	maxEntrySize = 64ull << 20;	// Largest single extracted file
		uint64_t	maxTotalRatio = 32;			// Total extracted size vs compressed archive size
	};

	[[ nodiscard ]] int extractArchive ( const std::string& dstFolder, const juce::MemoryBlock& mb, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles, juce::Thread* thread = nullptr, const Limits& limits = {} );

	// Converts a downloaded archive into a zip collection: a temp file
	// replaces dstZip atomically on success. A single wrapping "C64Music/"
	// folder is dropped. Returns the file count, -1 on failure
	[[ nodiscard ]] int convertArchiveToZip ( const juce::File& dstZip, const juce::MemoryBlock& mb, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles, juce::Thread* thread = nullptr, const Limits& limits = {} );

	// Stages every file of the archive into the ZipFolder overlay under
	// pathPrefix; the caller commits. Returns the file count, -1 on failure
	[[ nodiscard ]] int extractArchiveInto ( ZipFolder& zip, const juce::String& pathPrefix, const juce::MemoryBlock& mb, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles, const Limits& limits = {} );
}
//-----------------------------------------------------------------------------
