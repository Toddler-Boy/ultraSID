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
	addHeaderColumn ( columnId::length );
	addHeaderColumn ( columnId::liked );

	filterExactMatch = false;

	placeholderKey = "search/empty";
}
//-----------------------------------------------------------------------------

void GUI_Results::setDatabase ( std::vector<const Database::entry*> db )
{
	rowData.clear ();
	database = std::move ( db );
}
//-----------------------------------------------------------------------------

void GUI_Results::setUserDatabase ( std::vector<const Database::entry*> db )
{
	rowData.clear ();
	userDatabase = std::move ( db );
}
//-----------------------------------------------------------------------------

int GUI_Results::search ( const juce::String& str, const searchOptions options )
{
	if ( database.empty () && userDatabase.empty () )
		return 0;

	const auto	oldPattern = searchPattern;
	const auto	oldOptions = searchOpts;

	// Spaces separate words
	auto	words = juce::StringArray::fromTokens ( str.toLowerCase (), " ", "" );
	words.removeEmptyStrings ();

	// Convert words back into search pattern
	searchPattern = words.joinIntoString ( " " );
	searchOpts = options;

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

	auto matchesFilter = [ &options, &likes, &tags ] ( const Database::entry* entry ) -> bool
	{
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

	// Search tunes by filter only
	if ( words.isEmpty () && isAnyFilterUsed ( options ) )
	{
		// Search in HVSC database
		for ( auto entry : database )
			if ( matchesFilter ( entry ) )
				rowData.push_back ( entry );

		// Search in user database
		for ( auto entry : userDatabase )
			if ( matchesFilter ( entry ) )
				rowData.push_back ( entry );
	}
	else
	{
		std::vector<std::string>	substrings;
		for ( const auto& subStr : words )
			substrings.push_back ( subStr.toStdString () );

		auto findAnyString = [ &substrings ] ( const std::string_view searchStr )
		{
			for ( const auto& sub : substrings )
				if ( searchStr.find ( sub ) == std::string::npos )
					return false;

			return true;
		};

		// Search in HVSC database
		for ( auto entry : database )
			if ( matchesFilter ( entry ) && findAnyString ( entry->search ) )
				rowData.push_back ( entry );

		// Search in user database
		for ( auto entry : userDatabase )
			if ( matchesFilter ( entry ) && findAnyString ( entry->search ) )
				rowData.push_back ( entry );
	}
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

