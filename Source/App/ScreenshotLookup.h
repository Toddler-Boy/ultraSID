#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class ScreenshotLookup final
{
public:
	ScreenshotLookup () = default;

	// this
	void reload ();

	[[ nodiscard ]] std::vector<std::string> getScreenshots ( const std::string& tunename ) const;
	[[ nodiscard ]] std::string getDefaultScreenshot ( const std::string& tunename ) const;
	[[ nodiscard ]] static int getDefaultScreenshotIndex ( const std::vector<std::string>& screenshots );

	void addScreenshot ( const std::string& filename );
	void removeScreenshot ( const std::string& filename );

private:
	juce::CriticalSection	lutCs;
	std::unordered_map<std::string, std::vector<std::string>>	tuneFileToArtFiles;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( ScreenshotLookup )
};
//-----------------------------------------------------------------------------
