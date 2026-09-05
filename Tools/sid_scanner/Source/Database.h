#pragma once

#include <JuceHeader.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace libsidplayEZ { struct SharedPlayerConfig; }

//-----------------------------------------------------------------------------

class Database
{
public:
	Database () = default;

	// The scanner's corpus assumes at least this HVSC release
	static constexpr int	minHVSCVersion = 85;

	// Reads the release number off the attached hvscsource, then loads every
	// measurement file when the version is usable
	void attach ();
	void addEntry ( const std::string& md5, const unsigned int songNo, const float lufs, const float midLoudness, const bool filterUsed, const bool digiUsed, const bool looped, const uint32_t startMs, const std::string& settingsHash, const std::string& writeRates, const std::string& digiHint );
	void saveLUFS ();
	void saveFilterUsed ();
	void saveDigiUsed ();
	void saveLooped ();
	void saveStarts ();
	void saveSettings ();
	void saveWriteRates ();
	void saveDigiHints ();

	// For the hint save: hints of tunes digi-tunes.csv covers by now are
	// stale to-do items and get dropped
	void setSharedConfig ( std::shared_ptr<const libsidplayEZ::SharedPlayerConfig> config )	{	sharedConfig = std::move ( config );	}

	struct entry
	{
		std::vector<uint32_t>	lengths;
		std::vector<float>		loudness;
		std::vector<float>		midLoudness;		// loudness of the midband-filtered signal, "loudness/midLoudness" in the file
		std::vector<bool>		filterUsed;
		std::vector<bool>		digiUsed;
		std::vector<bool>		looped;
		std::vector<uint32_t>	startOffset;

		// Per-subtune settings fingerprints; empty = not recorded yet
		std::vector<std::string>	settings;

		// Per-subtune write-rate reports from unknown-mode scans; empty = none
		std::vector<std::string>	writeRates;

		// Per-subtune capture-mode suggestions derived from the write rates
		std::vector<std::string>	digiHints;

		// Carried over from the Songlengths entry, written into Songdelays.md5
		std::string				md5;
	};

	// Maps name and LUFs values
	std::map<std::string, entry>	db;

	int		hvscVersion = -1;

private:
	bool loadLUFS ();
	bool loadLengths ();
	bool loadStarts ();
	bool loadSettings ();
	bool loadWriteRates ();
	bool loadDigiHints ();

	// Load/save one bit per subtune, as "name=10110..." lines (filter & digi files)
	bool loadBits ( const char* filename, std::vector<bool> entry::* member );
	void saveBits ( const char* filename, std::vector<bool> entry::* member );

	// Shared "name=values" file skeleton; the lambdas handle the value payload:
	// parseValues ( entry&, const juce::String& ), formatValues ( const entry& ) -> line
	// (empty line = entry has no data and is skipped)
	template <typename ParseValues>
	bool loadFile ( const char* filename, ParseValues parseValues );
	template <typename FormatValues>
	void saveFile ( const char* filename, FormatValues formatValues );

	std::shared_ptr<const libsidplayEZ::SharedPlayerConfig>	sharedConfig;
};
//-----------------------------------------------------------------------------
