#pragma once

#include <JuceHeader.h>

#include <map>
#include <set>
#include <unordered_set>

//-----------------------------------------------------------------------------

// The factory Screenshots tree merged with the user's, names relative to
// Screenshots/: a user file replaces the factory file of the same name (hints aside)

class ScreenshotLookup final
{
public:
	ScreenshotLookup () = default;

	// this
	void reload ();

	[[ nodiscard ]] std::vector<std::string> getScreenshots ( const std::string& tunename ) const;
	[[ nodiscard ]] std::string getDefaultScreenshot ( const std::string& tunename ) const;
	[[ nodiscard ]] static int getDefaultScreenshotIndex ( const std::vector<std::string>& screenshots );

	// Every screenshot of the tune an art file belongs to, the file included
	[[ nodiscard ]] std::vector<std::string> getSiblings ( const std::string& artName ) const;

	// Highest _NN number the tune's screenshots use, 0 when it has none
	[[ nodiscard ]] int getLastNumber ( const std::string& tunename ) const;

	[[ nodiscard ]] bool exists ( const std::string& artName ) const;
	[[ nodiscard ]] bool isUserFile ( const std::string& artName ) const;

	// The name a picture goes by now: itself, or the sibling that differs only
	// in its hints (a curation renamed it); empty when it is gone
	[[ nodiscard ]] std::string currentName ( const std::string& artName ) const;

	// The bytes behind a name, from the user folder or the factory data
	[[ nodiscard ]] juce::MemoryBlock loadData ( const std::string& artName ) const;

	// The user folder's file for a name, whether it exists or not
	[[ nodiscard ]] juce::File getUserFile ( const std::string& artName ) const;

	// One folder of the merged tree ("" = root), entries in natural order
	// with their full relative paths
	struct listing
	{
		std::vector<std::string>	folders;
		std::vector<std::string>	files;
	};

	[[ nodiscard ]] listing list ( const std::string& folder ) const;

	void addScreenshot ( const std::string& filename );
	void removeScreenshot ( const std::string& filename );

private:
	void rebuildIndex ();

	juce::CriticalSection	lutCs;

	juce::File				userRoot;
	std::set<std::string>	names;
	std::unordered_set<std::string>	userNames;

	std::unordered_map<std::string, std::vector<std::string>>	tuneFileToArtFiles;
	std::map<std::string, listing>								tree;	// lowercase folder path

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( ScreenshotLookup )
};
//-----------------------------------------------------------------------------
