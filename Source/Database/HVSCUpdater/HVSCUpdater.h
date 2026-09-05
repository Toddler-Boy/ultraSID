#pragma once

#include <JuceHeader.h>

#include "std_lime/lime_string_utils.h"

#include "HVSCTree.h"
#include "updater_sidtune.h"

//-----------------------------------------------------------------------------

class HVSCUpdater
{
public:
	// Applies update/UpdateNN.hvs onto the tree; returns the error count.
	// The caller runs tree.finish() after the last clean script, so a chain
	// of updates commits a zip collection once
	[[ nodiscard ]] int update ( HVSCTree& tree, const int updateVersion, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles );

private:
	HVSCTree*		tree = nullptr;
	juce::String	keyword;

	int		errorCount = 0;

	std::vector<std::string>	lines;
	int							lineIndex = 0;

	std::unordered_set<std::string, lime::str::HashIgnoreCase, lime::str::EqualIgnoreCase>	caseFiles;

	[[ nodiscard ]] bool loadLines ( const juce::MemoryBlock& data );
	const std::string& getNextLine ();
	[[ nodiscard ]] std::string toFilename ( std::string in );
	[[ nodiscard ]] bool endOfLines () const { return lineIndex >= int ( lines.size () ); }

	mode_type	mode = NO_MODE;

	void logError ( const juce::String& message );
	bool fileExists ( const juce::String& rel, const bool withError = false );
	[[ nodiscard ]] bool fileExistsAsFile ( const juce::String& rel, const bool withError = false );
	bool deleteFile ( const juce::String& rel, const bool withError = false );
	[[ nodiscard ]] bool createDirectory ( const juce::String& rel );
	bool moveFileTo ( const juce::String& from, const juce::String& to );
};
//-----------------------------------------------------------------------------
