#pragma once

#include <optional>
#include <string>

#include "Database/Database.h"

//-----------------------------------------------------------------------------

namespace exportname
{
	// A marked result wraps every substituted value in markStart, the
	// placeholder letter, the value, markEnd, so a preview can color what
	// came from where
	constexpr char	markStart = '\x01';
	constexpr char	markEnd = '\x02';

	// Expands the export name template: {T} title, {A} author, {R} release,
	// {Y} its year ('?' digits become 'x'), {Q} the lower-cased quality,
	// {N}/{NN}/{NNN} the subtune number, dropped when it is the start tune.
	// Template slashes split the name into subfolders. Strips characters no
	// filesystem accepts; nullopt = unparsable template
	[[ nodiscard ]] std::optional<std::string> make ( std::string nameTemplate, const std::string& title, const std::string& author,
													  const std::string& release, const std::string& quality,
													  int subtune, int startTune, bool markTokens = false );

	// The full output file for a tune, the name expanded from the current
	// name-template and format preferences against the export-root setting;
	// "" when the name escapes the root or its folder can't be created
	[[ nodiscard ]] std::string makeExportPath ( const Database::entry& ent, const int subtune, const std::string& quality );
}
//-----------------------------------------------------------------------------
