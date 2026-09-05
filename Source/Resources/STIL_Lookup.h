#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class STILLookup final
{
public:
	STILLookup () = default;

	// this
	void load ();

	struct entry
	{
		std::string	folder;
		std::string	name;
		std::string	initials;
	};

	[[ nodiscard ]] entry findBestEntry ( const std::string& folder, const std::string& speaker );

private:
	std::vector<entry>	entries;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( STILLookup )
};
//-----------------------------------------------------------------------------

using GUI_STIL_blocks = std::list<std::tuple<juce::String, juce::String, juce::String>>;
//-----------------------------------------------------------------------------
