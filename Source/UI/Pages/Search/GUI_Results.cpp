#include <JuceHeader.h>

#include "GUI_Results.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Data/Likes.h"
#include "Data/Tags.h"
#include "UI/UI_Menus.h"

#include "../GUI_Pages.h"

//-----------------------------------------------------------------------------

GUI_Results::GUI_Results ( GUI_Pages& _pages )
	: pages ( _pages )
{
	setName ( "results" );

	addHeaderColumn ( columnId::animation );
	addHeaderColumn ( columnId::name, true );
	addHeaderColumn ( columnId::release, true );
	addHeaderColumn ( columnId::information, true );
	addHeaderColumn ( columnId::length, true );
	addHeaderColumn ( columnId::liked );

	filterExactMatch = false;

	placeholderKey = "search/empty";
}
//-----------------------------------------------------------------------------

void GUI_Results::setDatabase ( std::vector<const Database::entry*> db )
{
	rowData.clear ();
	unsorted.clear ();
	database = std::move ( db );
}
//-----------------------------------------------------------------------------

void GUI_Results::setUserDatabase ( std::vector<const Database::entry*> db )
{
	rowData.clear ();
	unsorted.clear ();
	userDatabase = std::move ( db );
}
//-----------------------------------------------------------------------------

void GUI_Results::sortOrderChanged ( int newSortColumnId, bool isForwards )
{
	// Column 0 = search order
	if ( sortKeyForColumn ( newSortColumnId ) == db::SortKey::none )
	{
		if ( unsorted.size () == rowData.size () )
			rowData = unsorted;

		return;
	}

	GUI_ListBox::sortOrderChanged ( newSortColumnId, isForwards );
}
//-----------------------------------------------------------------------------

struct yearRange
{
	int	from;
	int	to;
};

// A 4-char year as the lowest and highest year its '?' wildcards allow
static bool parseYear ( const std::string_view str, int& lo, int& hi )
{
	if ( str.size () != 4 )
		return false;

	lo = 0;
	hi = 0;

	for ( const auto c : str )
	{
		if ( c >= '0' && c <= '9' )
		{
			lo = lo * 10 + ( c - '0' );
			hi = hi * 10 + ( c - '0' );
		}
		else if ( c == '?' )
		{
			lo = lo * 10;
			hi = hi * 10 + 9;
		}
		else
			return false;
	}

	return true;
}
//-----------------------------------------------------------------------------

// "YYYY", "YYYY-", "-YYYY" or "YYYY-YYYY"; the ends may come in any order
static std::optional<yearRange> parseYearRange ( const std::string_view word )
{
	auto	leftLo = 0;
	auto	leftHi = 0;
	auto	rightLo = 9999;
	auto	rightHi = 9999;

	const auto	dash = word.find ( '-' );

	if ( dash == std::string_view::npos )
	{
		if ( ! parseYear ( word, leftLo, leftHi ) )
			return std::nullopt;

		return yearRange { leftLo, leftHi };
	}

	const auto	left = word.substr ( 0, dash );
	const auto	right = word.substr ( dash + 1 );

	if ( left.empty () && right.empty () )
		return std::nullopt;

	if ( ! left.empty () && ! parseYear ( left, leftLo, leftHi ) )
		return std::nullopt;

	if ( ! right.empty () && ! parseYear ( right, rightLo, rightHi ) )
		return std::nullopt;

	return yearRange { std::min ( leftLo, rightLo ), std::max ( leftHi, rightHi ) };
}
//-----------------------------------------------------------------------------

// The release year must be known to lie inside the range, "198?" is not in "1987"
static bool isInYearRange ( const std::string_view release, const yearRange range )
{
	auto	lo = 0;
	auto	hi = 0;

	return parseYear ( release.substr ( 0, 4 ), lo, hi ) && lo >= range.from && hi <= range.to;
}
//-----------------------------------------------------------------------------

int GUI_Results::search ( const juce::String& str, const searchOptions options )
{
	if ( database.empty () && userDatabase.empty () )
		return 0;

	const auto	oldPattern = searchPattern;
	const auto	oldOptions = searchOpts;

	// Spaces separate words, quotes keep a phrase together
	auto	words = juce::StringArray::fromTokens ( str.toLowerCase (), " ", "\"" );

	for ( auto i = words.size (); --i >= 0; )
		if ( words[ i ].unquoted ().isEmpty () )
			words.remove ( i );

	// The pattern keeps the year words so a year-only search has a non-empty key
	searchPattern = words.joinIntoString ( " " );
	searchOpts = options;

	// A word matches the whole search line unless a prefix limits it to one field
	struct searchTerm
	{
		std::string_view Database::entry::*	field;
		std::string							text;
	};

	static constexpr std::pair<const char*, std::string_view Database::entry::*>	fields[] =
	{
		{ "name", &Database::entry::lowerName },
		{ "author", &Database::entry::lowerAuthor },
		{ "path", &Database::entry::lowerFile },
		{ "publisher", &Database::entry::lowerPublisher },
	};

	std::vector<searchTerm>		terms;
	std::optional<yearRange>	yearFilter;

	for ( const auto& word : words )
	{
		const auto	quoted = word.startsWithChar ( '"' );
		const auto	colon = quoted ? -1 : word.indexOfChar ( ':' );
		const auto	prefix = colon > 0 ? word.substring ( 0, colon ) : juce::String ();
		const auto	known = std::ranges::find_if ( fields, [ &prefix ] ( const auto& f ) { return prefix == f.first; } );
		const auto	isYear = prefix == "year";
		const auto	text = ( known != std::end ( fields ) || isYear ? word.substring ( colon + 1 ) : word ).unquoted ().toStdString ();

		if ( text.empty () )
			continue;

		// A bare or "year:" range word is the year filter, the last one wins
		if ( isYear || ( ! quoted && colon < 0 ) )
		{
			if ( const auto range = parseYearRange ( text ) )
			{
				yearFilter = range;
				continue;
			}

			if ( isYear )
				continue;
		}

		terms.push_back ( { known != std::end ( fields ) ? known->second : &Database::entry::search, text } );
	}

	// Check if all options are false
	auto isAnyFilterUsed = [] ( const searchOptions& opts ) -> bool
	{
		return opts.mustBeLiked || opts.mustBePioneer || opts.mustBeWinner || opts.mustBeGem;
	};

	// Empty search
	if ( searchPattern.isEmpty () && ! isAnyFilterUsed ( options ) && rowData.size () != ( database.size () + userDatabase.size () ) )
	{
		rowData = database;
		rowData.insert ( rowData.end (), userDatabase.begin (), userDatabase.end () );
		unsorted = rowData;

		updateContent ();
		getHeader ().reSortTable ();

		return int ( rowData.size () );
	}

	// Same search as previous one
	if ( searchPattern == oldPattern && ! std::memcmp ( (const void*)&oldOptions, (const void*)&options, sizeof ( searchOptions ) ) && ! rowData.empty () )
		return int ( rowData.size () );

	// New search
	rowData.clear ();

	const juce::SharedResourcePointer<Likes>	likes;
	const juce::SharedResourcePointer<Tags>		tags;

	auto matchesFilter = [ &options, &likes, &tags, yearFilter ] ( const Database::entry* entry ) -> bool
	{
		if ( yearFilter && ! isInYearRange ( entry->release, *yearFilter ) )
			return false;

		if ( options.mustBeLiked && ! likes->isLiked ( entry->file ) )
			return false;

		if ( options.mustBePioneer && ! tags->isTagged ( "search/tag/pioneers", entry->file ) )
			return false;

		if ( options.mustBeWinner && ! tags->isTagged ( "search/tag/winners", entry->file ) )
			return false;

		if ( options.mustBeGem && ! tags->isTagged ( "search/tag/gems", entry->file ) )
			return false;

		return true;
	};

	auto matchesTerms = [ &terms ] ( const Database::entry* entry ) -> bool
	{
		for ( const auto& term : terms )
			if ( ( entry->*term.field ).find ( term.text ) == std::string_view::npos )
				return false;

		return true;
	};

	// Search in HVSC database
	for ( auto entry : database )
		if ( matchesFilter ( entry ) && matchesTerms ( entry ) )
			rowData.push_back ( entry );

	// Search in user database
	for ( auto entry : userDatabase )
		if ( matchesFilter ( entry ) && matchesTerms ( entry ) )
			rowData.push_back ( entry );

	unsorted = rowData;
	getHeader ().reSortTable ();

	return int ( rowData.size () );
}
//-----------------------------------------------------------------------------

GUI_Results::filterAvailability GUI_Results::getFilterAvailability () const
{
	const juce::SharedResourcePointer<Likes>	likes;
	const juce::SharedResourcePointer<Tags>		tags;

	filterAvailability	avail {};

	for ( const auto entry : rowData )
	{
		avail.liked = avail.liked || likes->isLiked ( entry->file );
		avail.pioneer = avail.pioneer || tags->isTagged ( "search/tag/pioneers", entry->file );
		avail.winner = avail.winner || tags->isTagged ( "search/tag/winners", entry->file );
		avail.gem = avail.gem || tags->isTagged ( "search/tag/gems", entry->file );

		if ( avail.liked && avail.pioneer && avail.winner && avail.gem )
			break;
	}

	return avail;
}
//-----------------------------------------------------------------------------

void GUI_Results::returnKeyPressed ( int lastRowSelected )
{
	pages.setCurrentPlaylist ( nullptr );
	const auto&	file = rowData[ lastRowSelected ]->file;
	pages.loadTune ( juce::String ( file.data (), file.size () ), 0, "search", -1 );
}
//-----------------------------------------------------------------------------

void GUI_Results::cellClicked ( int row, int columnId, const juce::MouseEvent& e )
{
	GUI_ListBox::cellClicked ( row, columnId, e );

	if ( ! e.mods.isPopupMenu () )
		return;

	auto	m = UI::newPopupMenu ( *this );

	const auto	rows = getSelectedRows ();
	const auto	selectedTunes = getTuneList ( rows, false );

	// A search row is a whole tune, and 0 already means its start song. Tags need the bare key
	juce::StringArray	tuneKeys;
	for ( const auto& tune : selectedTunes )
		tuneKeys.add ( tune + ",0" );

	// Add to playlist
	UI::menu_AddToPlaylist ( m, tuneKeys );

	// Go to artist
	m.addSeparator ();
	UI::menu_GoToFolder ( m, getTuneFolder ( rows ) );

	// Export track
	m.addSeparator ();
	UI::menu_ExportTrack ( m, tuneKeys );

	if ( buildinfo::isDeveloperMode () )
	{
		m.addSeparator ();
		UI::menu_ToggleTag ( m, selectedTunes );
	}

	UI::showMenuAtMouse ( m, *this );
}
//-----------------------------------------------------------------------------

