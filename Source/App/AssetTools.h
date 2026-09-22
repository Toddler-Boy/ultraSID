#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Screenshot files by their Screenshots-relative names: user files change in
// place, factory files get a shadowing user copy, or move through git in the developer tree

namespace assettools
{
	// Developer import: optimize (oxipng), move into Screenshots/<tune folder>/ and git-add
	void addScreenshots ( const juce::File& dataRoot, const std::string& tuneFilename, const juce::StringArray& filenames );

	// Rename an artwork file so its filename hint carries the new state
	void setBorderColor ( const std::string& artName, const int index );
	void toggleFirstLuma ( const std::string& artName );
	void toggleFirstLumaAll ( const std::vector<std::string>& artwork );
	void setScreenKind ( const std::string& artName, const int kind );
	void cycleScreenKind ( const std::string& artName );
	void toggleNTSC ( const std::string& artName );

	// User files always, factory files only in the developer tree
	[[ nodiscard ]] bool canDelete ( const std::string& artName );
	void deleteImage ( const std::string& artName );

	// A shown picture (absolute path or tree name) copied into the user tree:
	// as the tune's next _NN screenshot, or into a folder under its own name
	void keepForTune ( const juce::String& picture, const std::string& tuneKey );
	void saveToFolder ( const juce::String& picture, const std::string& folder );
}
//-----------------------------------------------------------------------------
