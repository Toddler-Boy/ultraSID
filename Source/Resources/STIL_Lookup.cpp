#include "STIL_Lookup.h"

#include "libSidplayEZ/src/EZ/tinyCSV.h"

#include "ultra-shared/Config/DataSource.h"

//-----------------------------------------------------------------------------

void STILLookup::load ()
{
	entries.clear ();

	const auto	str = datasource::loadText ( "UI/stil-names.csv" ).toStdString ();
	if ( str.empty () )
		return;

	auto	csv = libsidplayEZ::TinyCSV ();
	const auto	numEntries = csv.parseCSV ( str );

	for ( auto i = 0; i < numEntries; ++i )
		entries.push_back (	{ csv.get ( i, "folder" ), csv.get ( i, "name" ), csv.get ( i, "initials" ) } );
}
//-----------------------------------------------------------------------------

// findBestEntry: resolves a STIL quote speaker against the stilnames table.
// folder  = full HVSC path of the tune (UTF-8)
// speaker = initials or full name from the QUOTE block, already
//           converted Latin-1 -> UTF-8
// Always returns a displayable entry: on a miss, name carries the raw
// speaker and folder/initials are empty (no portrait will match).
STILLookup::entry STILLookup::findBestEntry ( const std::string& folder, const std::string& speaker )
{
	auto hasInitials = [ &speaker ] ( const entry& e )
	{
		for ( auto&& part : std::views::split ( std::string_view ( e.initials ), ';' ) )
			if ( std::string_view ( part ) == speaker )
				return true;

		return false;
	};

	// UTF-8 aware length: "JÅ" is 2 characters, not 3 bytes
	auto codepoints = [] ( std::string_view v )
	{
		return std::ranges::count_if ( v, [] ( unsigned char c ) { return ( c & 0xC0 ) != 0x80; } );
	};

	// Folder tier: longest CSV folder that prefixes the tune path and lists the speaker among its initials
	const entry*	best = nullptr;

	for ( const auto& e : entries )
		if ( folder.starts_with ( e.folder ) && hasInitials ( e ) && ( best == nullptr || e.folder.size () > best->folder.size () ) )
			best = &e;

	if ( best != nullptr )
		return *best;

	if ( codepoints ( speaker ) <= 4 )
	{
		// Global initials: accept only a unique match
		const entry*	unique = nullptr;
		for ( const auto& e : entries )
		{
			if ( hasInitials ( e ) )
			{
				if ( unique != nullptr )
				{
					unique = nullptr;
					break;
				}	// ambiguous
				unique = &e;
			}
		}

		if ( unique != nullptr )
			return *unique;
	}
	else
	{
		// Full name
		for ( const auto& e : entries )
			if ( e.name == speaker )
				return e;
	}

	return { {}, speaker, {} };		// unresolved: display as-is
}
//-----------------------------------------------------------------------------
