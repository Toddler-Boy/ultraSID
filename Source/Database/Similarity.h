#pragma once

#include <JuceHeader.h>

#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Database/Database.h"

//-----------------------------------------------------------------------------

// "More of the same": the subtunes that sound closest to a given one, from the
// sidflow-data vectors Tools/update_similarity.py converts (layout described there)
class Similarity
{
public:
	struct match
	{
		const Database::entry*	entry;
		int						subtune;
	};

	// The number of subtunes with a vector, 0 when the file is missing or broken
	int load ( const juce::MemoryBlock& mb, const Database& database );

	[[ nodiscard ]] bool contains ( const Database::entry& entry ) const;

	// Closest first, one subtune per tune, near-copies of a better match left
	// out. Subtune 0 = the tune's default
	[[ nodiscard ]] std::vector<match> find ( std::string_view lowerKey, int subtune, int count,
											  const std::function<bool ( const match& seed, const match& candidate )>& accept ) const;

private:
	static constexpr int	centroids = 256;

	struct row
	{
		const Database::entry*	entry;
		int						subtune;
		float					invNorm;
	};

	void decode ( size_t rowIndex, float* dst ) const;

	int						dims = 0;
	std::vector<float>		codebook;	// dims x centroids
	std::vector<row>		rows;
	std::vector<uint8_t>	codes;		// dims per row

	// Database::entry::lowerFile -> first row, row count
	std::unordered_map<std::string_view, std::pair<uint32_t, uint32_t>>	tuneRows;
};
//-----------------------------------------------------------------------------
