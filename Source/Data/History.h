#pragma once

#include <JuceHeader.h>

#include <string>
#include <string_view>
#include <vector>

//-----------------------------------------------------------------------------

// The play history, newest first, persisted as history.csv in the user folder.
// Entries name the tune by its database key and outlive the tune itself
class History final
{
public:
	struct Entry
	{
		std::string	file;
		int			subtune = 1;
		juce::Time	time;
	};

	// The newest maxRetainedItems always stay, entries beyond that age out
	static constexpr auto	maxRetainedItems = 300;
	static constexpr auto	maxRetainedAgeDays = 365.0;

	History () = default;

	void setRoot ( const juce::File& root );

	[[ nodiscard ]] const std::vector<Entry>& getEntries () const	{	return entries;	}
	[[ nodiscard ]] int numEntries () const							{	return int ( entries.size () );	}

	// Records a play now; an earlier entry of the same tune moves to the front
	void add ( std::string_view tune, int subtune );

	// Indices into getEntries (), any order
	void remove ( std::vector<int> indices );

	void clearOlderThan ( double days );
	void clearAll ();

private:
	void load ();
	void save ();

	void insert ( std::string_view tune, int subtune, const juce::Time& time );

	juce::File			root;
	std::vector<Entry>	entries;
};
//-----------------------------------------------------------------------------
