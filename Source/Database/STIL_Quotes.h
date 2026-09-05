#pragma once

#include <string>
#include <vector>

//-----------------------------------------------------------------------------

// Splits a STIL comment body into quote and comment segments, attributing
// quotes to their speakers by shape (STIL has no formal quote syntax).
// Pure text processing, no JUCE, no UI.

struct StilSegment
{
	std::string type;      // "QUOTE" or "COMMENT"
	std::string text;      // full text; QUOTE comes without its enclosing quote characters
	std::string speaker;   // cleaned attribution ("JCH", "Anthony Lees"); empty for
						   // COMMENT segments and unattributed quotes
};

namespace stilq
{
	[[ nodiscard ]] std::vector<StilSegment> splitCommentQuotes ( const std::string& bodyIn );
}
//-----------------------------------------------------------------------------
