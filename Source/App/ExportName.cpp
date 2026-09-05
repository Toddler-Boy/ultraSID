#include <JuceHeader.h>

#include <algorithm>
#include <fmt/format.h>

#include "ExportName.h"

#include "libSidplayEZ/src/stringutils.h"

#include "std_lime/lime_string_utils.h"

#include "Config/Preferences.h"
#include "Config/Settings.h"

//-----------------------------------------------------------------------------

std::optional<std::string> exportname::make ( std::string nameTemplate, const std::string& _title, const std::string& _author,
											  const std::string& _release, const std::string& _quality,
											  const int subtune, const int startTune, const bool markTokens )
{
	// Tune text fields are not reliably clean, user tunes carry stray spaces.
	// Separators leave the values here: only template-authored ones may split
	// folders, a slash inside an author field must not
	auto clean = [] ( const std::string& s )
	{
		return juce::String ( stringutils::trim ( s ) ).removeCharacters ( "\\/" ).toStdString ();
	};

	auto mark = [ markTokens ] ( const char token, const std::string& s ) -> std::string
	{
		if ( ! markTokens || s.empty () )
			return s;

		return std::string { markStart, token } + s + markEnd;
	};

	const auto	title = mark ( 'T', clean ( _title ) );
	const auto	author = mark ( 'A', clean ( _author ) );
	const auto	quality = mark ( 'Q', juce::String ( clean ( _quality ) ).toLowerCase ().toStdString () );
	const auto	release = clean ( _release );

	if ( nameTemplate.empty () )
	{
		const juce::SharedResourcePointer<Preferences>	preferences;
		nameTemplate = preferences->getDefault<std::string> ( "export/name-template" );
	}

	lime::str::replaceAll ( nameTemplate, "{T}", "{0}" );
	lime::str::replaceAll ( nameTemplate, "{A}", "{1}" );
	lime::str::replaceAll ( nameTemplate, "{R}", "{2}" );
	lime::str::replaceAll ( nameTemplate, "{Y}", "{4}" );
	lime::str::replaceAll ( nameTemplate, "{Q}", "{5}" );

	// The release year, uncertain digits written the discography way (198x).
	// User tunes hold anything in this field, so it only counts as a year
	// when all four characters are digits or '?'
	auto	year = release.substr ( 0, 4 );

	if ( year.size () < 4 || ! std::ranges::all_of ( year, [] ( const char c ) { return ( c >= '0' && c <= '9' ) || c == '?'; } ) )
		year.clear ();

	std::ranges::replace ( year, '?', 'x' );
	year = mark ( 'Y', year );

	const auto	markedRelease = mark ( 'R', release );

	if ( subtune == startTune )
	{
		lime::str::replaceAll ( nameTemplate, "{N}", "" );
		lime::str::replaceAll ( nameTemplate, "{NN}", "" );
		lime::str::replaceAll ( nameTemplate, "{NNN}", "" );
	}
	else
	{
		lime::str::replaceAll ( nameTemplate, "{N}", mark ( 'N', "#{3}" ) );
		lime::str::replaceAll ( nameTemplate, "{NN}", mark ( 'N', "#{3:02d}" ) );
		lime::str::replaceAll ( nameTemplate, "{NNN}", mark ( 'N', "#{3:03d}" ) );
	}

	std::string	name;

	try
	{
		name = fmt::vformat ( nameTemplate, fmt::make_format_args ( title, author, markedRelease, subtune, year, quality ) );
	}
	catch ( const fmt::format_error& )
	{
		// Hand-edited template with stray braces
		return std::nullopt;
	}

	// An emptied placeholder can leave doubled spaces behind, collapse them
	while ( lime::str::replaceAll ( name, "  ", " " ) )
	{
	}

	// Template separators split into subfolders ('\' spelled either way).
	// Every segment sheds illegal filename characters and outer whitespace;
	// folder segments also lose the trailing dots Windows forbids, which
	// inherently drops "." and ".." traversal segments. Emptied segments
	// vanish, so stray or doubled separators cannot derail the path
	std::ranges::replace ( name, '\\', '/' );

	auto	segments = juce::StringArray::fromTokens ( juce::String ( name ), "/", "" );

	for ( auto i = 0; i < segments.size (); ++i )
	{
		auto	segment = segments[ i ].removeCharacters ( ":*?\"<>|" ).trim ();

		if ( i < segments.size () - 1 )
			segment = segment.trimCharactersAtEnd ( ". " );

		segments.set ( i, segment );
	}

	segments.removeEmptyStrings ();

	return segments.joinIntoString ( "/" ).toStdString ();
}
//-----------------------------------------------------------------------------

std::string exportname::makeExportPath ( const Database::entry& ent, const int subtune, const std::string& quality )
{
	const juce::SharedResourcePointer<Preferences>	preferences;

	const auto	utf8Name = stringutils::trim ( stringutils::extendedASCIItoUTF8 ( ent.name ) );
	const auto	utf8Author = stringutils::trim ( stringutils::extendedASCIItoUTF8 ( ent.author ) );

	auto	made = make ( preferences->get<std::string> ( "export/name-template" ),
						  utf8Name, utf8Author, stringutils::extendedASCIItoUTF8 ( ent.release ),
						  quality, subtune, ent.startTune );

	if ( ! made )
	{
		// Hand-edited template with stray braces, fall back to a fixed name
		Z_ERR ( "Invalid export name template: " << preferences->get<juce::String> ( "export/name-template" ) );

		auto	fallback = utf8Author + " - " + utf8Name;
		if ( subtune != ent.startTune )
			fallback += " #" + std::to_string ( subtune );

		made = juce::String ( stringutils::trim ( fallback ) ).removeCharacters ( "\\/:*?\"<>|" ).toStdString ();
	}

	const juce::SharedResourcePointer<Settings>	settings;

	const auto	saveRoot = juce::File ( settings->get<juce::String> ( "paths/export" ) );
	const auto	saveFile = saveRoot.getChildFile ( juce::String ( *made ) + "." + preferences->get<juce::String> ( "export/format" ).toLowerCase () );

	// Belt to the template's segment cleanup: whatever the name expanded to,
	// it stays inside the chosen export folder
	if ( ! saveFile.isAChildOf ( saveRoot ) )
	{
		Z_ERR ( "Export name escapes the export folder: " << *made );
		return {};
	}

	// Template separators may have added subfolders under the export root
	saveFile.getParentDirectory ().createDirectory ();
	if ( ! saveFile.getParentDirectory ().isDirectory () )
	{
		Z_INFO ( "Can't write to " << saveFile.getParentDirectory ().getFullPathName ().quoted () );
		return {};
	}

	return saveFile.getFullPathName ().toStdString ();
}
//-----------------------------------------------------------------------------
