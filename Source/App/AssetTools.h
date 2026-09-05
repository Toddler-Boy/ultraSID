#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Developer-mode screenshot asset management. These rename/delete artwork
// files in the Data repository and mirror every change with git (mv/rm/add)
// by shelling out, so the working copy stays consistent. Callers are
// responsible for the developer-mode gate.

namespace assettools
{
	// Optimize (oxipng), move into Screenshots/<tune folder>/ and git-add
	void addScreenshots ( const juce::File& dataRoot, const std::string& tuneFilename, const juce::StringArray& filenames );

	// Rename an artwork file so its filename hint carries the new state
	void setBorderColor ( const juce::File& imageFile, const int index );
	void toggleFirstLuma ( const juce::File& imageFile );
	void toggleFirstLumaAll ( const juce::File& dataRoot, const std::vector<std::string>& artwork );
	void toggleThumbnail ( const juce::File& imageFile );

	void deleteImage ( const juce::File& imageFile );
}
//-----------------------------------------------------------------------------
