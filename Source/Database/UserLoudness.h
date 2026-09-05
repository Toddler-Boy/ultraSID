#pragma once

#include <JuceHeader.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

//
// Live-measurement cache for user tunes, kept in the scanner's SID_LUFS
// scheme ("name=lufs/mid" per subtune, -96.0/-96.0 = unmeasured): a user tune
// measures once and plays correctly leveled from then on. Message-thread only.
//
class UserLoudness
{
public:
	UserLoudness ();

	// The cache key: the tune key's filename inside the user folder, no
	// location marker, no extension
	[[ nodiscard ]] static std::string keyFor ( const std::string& tuneKey );

	// ( integrated, midband ) in LUFS, -96/-96 when never measured
	[[ nodiscard ]] std::pair<float, float> get ( const std::string& key, unsigned int songNo ) const;

	void store ( const std::string& key, unsigned int songNo, float lufs, float midLufs );

private:
	void save () const;

	juce::File	file;
	std::map<std::string, std::vector<std::pair<float, float>>>	db;
};
//-----------------------------------------------------------------------------
