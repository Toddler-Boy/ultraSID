#pragma once

#include <algorithm>
#include <cctype>
#include <string>

//-----------------------------------------------------------------------------

// Display-name rules shared by the db builder and the user-tune scan: tunes
// with a "<?>" HVSC placeholder title, and same-named tunes within one
// folder, show their filename instead

namespace tunenames
{
	[[ nodiscard ]] inline bool isPlaceholder ( const std::string& name )
	{
		return name.empty () || name == "<?>";
	}
	//-------------------------------------------------------------------------

	// Duplicate detection compares names case-insensitively
	[[ nodiscard ]] inline std::string folded ( std::string name )
	{
		for ( auto& ch : name )
			ch = char ( std::tolower ( (unsigned char)ch ) );

		return name;
	}
	//-------------------------------------------------------------------------

	// ".../Worktune_01.sid" -> "Worktune 01"; only the known extension comes
	// off (HVSC filenames carry real dots), trailing "The" gets its comma back
	[[ nodiscard ]] inline std::string stemName ( const std::string& fileKey )
	{
		const auto	slash = fileKey.rfind ( '/' );
		auto	stem = slash == std::string::npos ? fileKey : fileKey.substr ( slash + 1 );

		if ( stem.size () > 4 && ( stem.ends_with ( ".sid" ) || stem.ends_with ( ".SID" ) ) )
			stem.resize ( stem.size () - 4 );

		// A letterless stem ("Worktunes/02") leaves the identity to its folder
		const auto	hasLetter = std::any_of ( stem.begin (), stem.end (),
											  [] ( unsigned char ch ) { return std::isalpha ( ch ) != 0; } );

		if ( ! hasLetter && slash != std::string::npos && slash > 0 )
			if ( const auto parent = fileKey.rfind ( '/', slash - 1 ); parent != std::string::npos )
				stem = fileKey.substr ( parent + 1, slash - parent - 1 ) + " " + stem;

		std::replace ( stem.begin (), stem.end (), '_', ' ' );

		if ( stem.ends_with ( " The" ) )
			stem.replace ( stem.size () - 4, 4, ", The" );

		return stem;
	}
}
//-----------------------------------------------------------------------------
