#include <algorithm>

#include "History.h"

#include "libSidplayEZ/src/EZ/tinyCSV.h"

#include "ultra-shared/Helpers/FileUtils.h"

#include "Database/Database.h"

//-----------------------------------------------------------------------------

// The tune key is written quoted, it can contain commas
static constexpr auto	csvHeader = "tuneKey,subtune,date\n";
static constexpr auto	fileName = "history.csv";

//-----------------------------------------------------------------------------

void History::setRoot ( const juce::File& _root )
{
	root = _root;
	load ();
}
//-----------------------------------------------------------------------------

void History::load ()
{
	entries.clear ();

	if ( root == juce::File () )
		return;

	const auto	file = root.getChildFile ( fileName );
	if ( ! file.existsAsFile () )
		return;

	libsidplayEZ::TinyCSV	csv;
	const auto	numRows = csv.parseCSV ( file.loadFileAsString ().toStdString () );

	if ( const auto& err = csv.getError (); ! err.empty () )
		Z_ERR ( "History: " << err );

	// The file is newest-first, and each insert goes to the front
	for ( auto i = numRows - 1; i >= 0; --i )
	{
		const auto	tune = csv.get ( i, "tuneKey", "" );
		if ( tune.empty () )
			continue;

		insert ( tune, csv.get<int> ( i, "subtune" ), juce::Time::fromISO8601 ( juce::String ( csv.get ( i, "date", "" ) ) ) );
	}
}
//-----------------------------------------------------------------------------

void History::save ()
{
	if ( root == juce::File () )
		return;

	std::string	list = csvHeader;

	for ( const auto& entry : entries )
	{
		juce::StringArray	fields;

		fields.add ( juce::String ( entry.file ).quoted () );
		fields.add ( juce::String ( entry.subtune ) );
		fields.add ( entry.time.toISO8601 ( true ) );

		list += fields.joinIntoString ( "," ).toStdString ();
		list += '\n';
	}

	root.createDirectory ();

	fileutils::replaceFile ( root.getChildFile ( fileName ), list.c_str (), list.size () );
}
//-----------------------------------------------------------------------------

void History::insert ( const std::string_view tune, const int subtune, const juce::Time& time )
{
	// Stored under the database's own key when the tune resolves
	const auto	ent = db::findDatabaseEntry ( std::string ( tune ) );
	const auto	file = ent ? std::string ( ent->file ) : std::string ( tune );

	entries.insert ( entries.begin (), { file, subtune, time } );

	// The same tune played before sits further down, once
	const auto	older = std::find_if ( entries.begin () + 1, entries.end (), [ & ] ( const Entry& e )
	{
		return e.file == file && e.subtune == subtune;
	} );

	if ( older != entries.end () )
		entries.erase ( older );

	const auto	cutoff = juce::Time::getCurrentTime () - juce::RelativeTime::days ( maxRetainedAgeDays );
	while ( entries.size () > maxRetainedItems && entries.back ().time < cutoff )
		entries.pop_back ();
}
//-----------------------------------------------------------------------------

void History::add ( const std::string_view tune, const int subtune )
{
	insert ( tune, subtune, juce::Time::getCurrentTime () );
	save ();
}
//-----------------------------------------------------------------------------

void History::remove ( std::vector<int> indices )
{
	std::ranges::sort ( indices, std::greater<> () );

	for ( const auto index : indices )
		if ( juce::isPositiveAndBelow ( index, int ( entries.size () ) ) )
			entries.erase ( entries.begin () + index );

	save ();
}
//-----------------------------------------------------------------------------

void History::clearOlderThan ( const double days )
{
	const auto	cutoff = juce::Time::getCurrentTime () - juce::RelativeTime::days ( days );

	// Newest-first, so everything past the cutoff sits at the tail
	while ( ! entries.empty () && entries.back ().time < cutoff )
		entries.pop_back ();

	save ();
}
//-----------------------------------------------------------------------------

void History::clearAll ()
{
	entries.clear ();
	save ();
}
//-----------------------------------------------------------------------------
