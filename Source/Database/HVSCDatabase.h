#pragma once

#include <JuceHeader.h>

#include "std_lime/lime_string_utils.h"

#include "Config/FilePaths.h"

//-----------------------------------------------------------------------------

class HVSC_database final : private juce::Thread
{
public:
	HVSC_database ();
	~HVSC_database () override;

	// Reads the installed release number off the current hvscsource root
	void attach ();

	void load ( std::function<void ()> callback );
	[[ nodiscard ]] std::string getErrorString () const	{	const juce::ScopedLock sl ( dbLock );	return errorCode;	}
	[[ nodiscard ]] int getHVSCVersion () const	{	return hvscVersion;	}

	using STIL_block = std::vector<std::pair<std::string, std::string>>;
	using STIL_tune = std::unordered_map<int, STIL_block>;

	[[ nodiscard ]] std::optional<STIL_block> getSTILEntry ( const std::string& name, const int tune = 0 );
	[[ nodiscard ]] uint32_t getLengthMs ( std::string_view name, const int tune ) const;

	// Milliseconds of silence before the music starts, 0 = starts right away
	[[ nodiscard ]] uint32_t getStartMs ( std::string_view name, const int tune ) const;

	// Whether any subtune of the file starts delayed. Takes a pre-folded tune
	// key (Database::entry::lowerFile), so per-paint callers don't fold at all
	[[ nodiscard ]] bool hasDelays ( const std::string_view lowerName ) const
	{
		const juce::ScopedLock	sl ( dbLock );

		return startDB.contains ( filepaths::stripLocationMarker ( lowerName ) );
	}

private:
	void run () override;
	std::function<void ()>	callback;

	// this
	std::string	errorCode = "path not set";
	int			hvscVersion = -1;

	void loadLengths ();
	void loadStarts ();
	void loadSTIL ();
	void loadBugs ();

	mutable juce::CriticalSection	dbLock;

	// Maps tune keys (path past the marker) to per-subtune milliseconds
	using msMap = std::unordered_map<std::string, std::vector<uint32_t>, lime::str::TransparentHash, std::equal_to<>>;

	[[ nodiscard ]] uint32_t lookupMs ( const msMap& db, std::string_view name, const int tune ) const;

	// Maps Folder/Filenames to STIL_tune entries
	std::unordered_map<std::string, STIL_tune>	stilDB;

	// Maps filenames to length database
	msMap	lengthDB;

	// Maps filenames to per-subtune delayed-start offsets (only tunes that have one)
	msMap	startDB;
};
//-----------------------------------------------------------------------------
