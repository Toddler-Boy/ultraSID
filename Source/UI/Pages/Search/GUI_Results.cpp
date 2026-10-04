#include <JuceHeader.h>

#include <functional>

#include "GUI_Results.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/ScreenshotLookup.h"
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
		{
			rowData = unsorted;
			rowSubtune = unsortedSubtune;
		}

		return;
	}

	GUI_ListBox::sortOrderChanged ( newSortColumnId, isForwards );
}
//-----------------------------------------------------------------------------

struct intRange
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

// No SID can be older than the C64 itself
static constexpr auto	firstYear = 1982;

static int currentYear ()
{
	static const auto	year = juce::Time::getCurrentTime ().getYear ();

	return year;
}
//-----------------------------------------------------------------------------

// A year a SID could be from, or a decade as "198?"; any other number is search text
static bool parseSearchYear ( const std::string_view str, int& lo, int& hi )
{
	if ( str.find ( '?' ) < 3 || ! parseYear ( str, lo, hi ) )
		return false;

	return hi >= firstYear && lo <= currentYear ();
}
//-----------------------------------------------------------------------------

// "YYYY", "YYYY-", "-YYYY" or "YYYY-YYYY"; the ends may come in any order
static std::optional<intRange> parseYearRange ( const std::string_view word )
{
	auto	leftLo = firstYear;
	auto	leftHi = firstYear;
	auto	rightLo = currentYear ();
	auto	rightHi = currentYear ();

	const auto	dash = word.find ( '-' );

	if ( dash == std::string_view::npos )
	{
		if ( ! parseSearchYear ( word, leftLo, leftHi ) )
			return std::nullopt;

		return intRange { leftLo, leftHi };
	}

	const auto	left = word.substr ( 0, dash );
	const auto	right = word.substr ( dash + 1 );

	if ( left.empty () && right.empty () )
		return std::nullopt;

	if ( ! left.empty () && ! parseSearchYear ( left, leftLo, leftHi ) )
		return std::nullopt;

	if ( ! right.empty () && ! parseSearchYear ( right, rightLo, rightHi ) )
		return std::nullopt;

	return intRange { std::min ( leftLo, rightLo ), std::max ( leftHi, rightHi ) };
}
//-----------------------------------------------------------------------------

// "N", "N+", "N-", "-N" or "N-M", up to two digits each
static std::optional<intRange> parseSidRange ( const std::string_view word )
{
	constexpr auto	mostSids = 99;

	const auto	number = [] ( const std::string_view s, int& out )
	{
		if ( s.empty () || s.size () > 2 )
			return false;

		out = 0;
		for ( const auto c : s )
		{
			if ( c < '0' || c > '9' )
				return false;

			out = out * 10 + ( c - '0' );
		}

		return true;
	};

	auto	from = 1;
	auto	to = mostSids;

	if ( word.ends_with ( '+' ) )
		return number ( word.substr ( 0, word.size () - 1 ), from ) ? std::optional ( intRange { from, mostSids } ) : std::nullopt;

	const auto	dash = word.find ( '-' );

	if ( dash == std::string_view::npos )
		return number ( word, from ) ? std::optional ( intRange { from, from } ) : std::nullopt;

	const auto	left = word.substr ( 0, dash );
	const auto	right = word.substr ( dash + 1 );

	if ( left.empty () && right.empty () )
		return std::nullopt;

	if ( ! left.empty () && ! number ( left, from ) )
		return std::nullopt;

	if ( ! right.empty () && ! number ( right, to ) )
		return std::nullopt;

	return intRange { from, to };
}
//-----------------------------------------------------------------------------

// a and b differ by at most one wrong, missing, extra or swapped letter
static bool withinOneEdit ( std::string_view a, std::string_view b )
{
	if ( a.size () == b.size () )
	{
		size_t	i = 0;
		while ( i < a.size () && a[ i ] == b[ i ] )
			++i;

		if ( i == a.size () || a.substr ( i + 1 ) == b.substr ( i + 1 ) )
			return true;

		return i + 1 < a.size () && a[ i ] == b[ i + 1 ] && a[ i + 1 ] == b[ i ] && a.substr ( i + 2 ) == b.substr ( i + 2 );
	}

	if ( a.size () + 1 == b.size () )
		std::swap ( a, b );

	if ( a.size () != b.size () + 1 )
		return false;

	size_t	i = 0;
	while ( i < b.size () && a[ i ] == b[ i ] )
		++i;

	return a.substr ( i + 1 ) == b.substr ( i );
}
//-----------------------------------------------------------------------------

struct textHit
{
	size_t	pos;
	size_t	len;
};

// Words of four letters or more may be one typo off once the exact pass
// found nothing
static constexpr auto	shortestFuzzyWord = 4;

// Where field holds text with at most one edit; one edit leaves one of the
// three pieces intact, so only their exact hits get the full check
static std::optional<textHit> fuzzyFind ( const std::string_view field, const std::string_view text )
{
	const auto	m = text.size ();
	const auto	k = m / 2;

	const auto	fromPiece = [ & ] ( const size_t a, const size_t b ) -> std::optional<textHit>
	{
		const auto	piece = text.substr ( a, b - a );

		for ( auto p = field.find ( piece ); p != std::string_view::npos; p = field.find ( piece, p + 1 ) )
			for ( auto s = ptrdiff_t ( p ) - ptrdiff_t ( a ) - 1; s <= ptrdiff_t ( p - a ) + 1; ++s )
			{
				if ( s < 0 )
					continue;

				for ( auto len = m - 1; len <= m + 1; ++len )
					if ( size_t ( s ) + len <= field.size () && withinOneEdit ( field.substr ( size_t ( s ), len ), text ) )
						return textHit { size_t ( s ), len };
			}

		return std::nullopt;
	};

	if ( const auto hit = fromPiece ( 0, k ) )
		return hit;

	if ( const auto hit = fromPiece ( k, m ) )
		return hit;

	return fromPiece ( k + 1, m );
}
//-----------------------------------------------------------------------------

// The release year must be known to lie inside the range, "198?" is not in "1987"
static bool isInYearRange ( const std::string_view release, const intRange range )
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

	// A hit ranks by field, then by the tune having screenshots, then by how
	// much of the field the word covers: the whole field, a word start, anywhere
	struct searchField
	{
		const char*							prefix;
		std::string_view Database::entry::*	view;
		int									rank;
	};

	static constexpr searchField	fields[] =
	{
		{ "name", &Database::entry::searchName, 3 },
		{ "author", &Database::entry::searchAuthor, 2 },
		{ "publisher", &Database::entry::searchPublisher, 1 },
		{ "path", &Database::entry::searchFile, 0 },
	};

	// The cover tier of text in field, 0 when absent
	const auto	coverOf = [ this ] ( const std::string_view field, const std::string& text )
	{
		auto	hit = std::optional<textHit> ();

		if ( const auto pos = field.find ( text ); pos != std::string_view::npos )
			hit = textHit { pos, text.size () };
		else if ( fuzzy && text.size () >= shortestFuzzyWord )
			hit = fuzzyFind ( field, text );

		if ( ! hit )
			return 0;

		return hit->len == field.size () ? 3 : hit->pos == 0 || field[ hit->pos - 1 ] == ' ' ? 2 : 1;
	};

	const auto	rankIn = [ &coverOf ] ( const std::string_view field, const std::string& text, const int fieldRank )
	{
		const auto	cover = coverOf ( field, text );

		return cover ? fieldRank * 8 + cover : 0;
	};

	constexpr auto	pictureRank = 4;

	// Folds like the fields, code points beyond the sorting table are dropped
	const auto	foldWord = [] ( const juce::String& word )
	{
		std::string	latin;

		for ( const auto cp : word )
			if ( cp < 256 )
				latin += char ( cp );

		std::string	folded ( latin.size (), '\0' );
		folded.resize ( db::foldSearch ( latin, folded.data () ) );

		return folded;
	};

	// "has:" asks whether any subtune or attachment carries it, "is:" states a
	// fact about the file; an unknown value is ignored
	static constexpr std::pair<const char*, bool ( * ) ( const Database::entry& )>	keywords[] =
	{
		{ "has:digi", [] ( const Database::entry& e ) { return e.hasAnyDigi (); } },
		{ "has:filter", [] ( const Database::entry& e ) { return e.hasAnyFilter (); } },
		{ "has:oneshot", [] ( const Database::entry& e ) { return e.hasAnyOneShot (); } },
		{ "is:pal", [] ( const Database::entry& e ) { return e.isPAL (); } },
		{ "is:ntsc", [] ( const Database::entry& e ) { return e.isNTSC (); } },
		{ "is:6581", [] ( const Database::entry& e ) { return ( e.chipModels () & 1 ) != 0; } },
		{ "is:8580", [] ( const Database::entry& e ) { return ( e.chipModels () & 2 ) != 0; } },
	};

	terms.clear ();
	fuzzy = false;

	std::vector<bool ( * ) ( const Database::entry& )>	tests;
	std::optional<intRange>								yearFilter;
	std::optional<intRange>								sidFilter;
	auto												mustHaveScreenshots = false;

	for ( const auto& word : words )
	{
		const auto	quoted = word.startsWithChar ( '"' );

		if ( ! quoted && word.startsWith ( "sids:" ) )
		{
			if ( const auto range = parseSidRange ( word.substring ( 5 ).toStdString () ) )
				sidFilter = range;

			continue;
		}

		if ( ! quoted && ( word.startsWith ( "has:" ) || word.startsWith ( "is:" ) ) )
		{
			if ( word == "has:screenshot" || word == "has:screenshots" )
				mustHaveScreenshots = true;
			else if ( const auto k = std::ranges::find_if ( keywords, [ &word ] ( const auto& kw ) { return word == kw.first; } ); k != std::end ( keywords ) )
				tests.push_back ( k->second );

			continue;
		}

		const auto	colon = quoted ? -1 : word.indexOfChar ( ':' );
		const auto	prefix = colon > 0 ? word.substring ( 0, colon ) : juce::String ();
		const auto	known = std::ranges::find_if ( fields, [ &prefix ] ( const auto& f ) { return prefix == f.prefix; } );
		const auto	isYear = prefix == "year";
		const auto	isStil = prefix == "stil";
		const auto	typed = ( known != std::end ( fields ) || isYear || isStil ? word.substring ( colon + 1 ) : word ).unquoted ();
		const auto	text = typed.toStdString ();

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

		if ( auto folded = foldWord ( typed ); ! folded.empty () )
			terms.push_back ( { known != std::end ( fields ) ? known->view : nullptr, isStil, std::move ( folded ) } );
	}

	const auto	stilTerms = std::ranges::count_if ( terms, &searchTerm::stil );

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
		rowSubtune.assign ( rowData.size (), 0 );
		stilRows.clear ();
		unsorted = rowData;
		unsortedSubtune = rowSubtune;
		closeMatch = false;

		updateContent ();
		getHeader ().reSortTable ();

		return int ( rowData.size () );
	}

	const juce::SharedResourcePointer<ScreenshotLookup>	screenshots;

	// Same search as previous one
	if ( searchPattern == oldPattern && ! std::memcmp ( (const void*)&oldOptions, (const void*)&options, sizeof ( searchOptions ) )
		 && searchScreenshots == screenshots->getGeneration () && searchStil == hvscDB->getSTILGeneration () && ! rowData.empty () )
		return int ( rowData.size () );

	// New search
	rowData.clear ();
	rowSubtune.clear ();
	stilRows.clear ();
	searchScreenshots = screenshots->getGeneration ();
	searchStil = hvscDB->getSTILGeneration ();

	const juce::SharedResourcePointer<Likes>	likes;
	const juce::SharedResourcePointer<Tags>		tags;

	auto matchesFilter = [ &options, &likes, &tags, &tests, yearFilter, sidFilter ] ( const Database::entry* entry ) -> bool
	{
		if ( yearFilter && ! isInYearRange ( entry->release, *yearFilter ) )
			return false;

		if ( sidFilter && ( entry->sidCount () < sidFilter->from || entry->sidCount () > sidFilter->to ) )
			return false;

		if ( ! std::ranges::all_of ( tests, [ entry ] ( const auto test ) { return test ( *entry ); } ) )
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

	// The ranks of every word's best hit added up, nullopt when a word has none
	auto scoreTerms = [ this, &rankIn ] ( const Database::entry* entry ) -> std::optional<int>
	{
		auto	score = 0;

		for ( const auto& term : terms )
		{
			if ( term.stil )
				continue;

			auto	best = 0;

			for ( const auto& f : fields )
				if ( ! term.field || term.field == f.view )
					best = std::max ( best, rankIn ( entry->*f.view, term.text, f.rank ) );

			if ( ! best )
				return std::nullopt;

			score += best;
		}

		return score;
	};

	// One row per subtune, from its best line holding every "stil:" word
	struct stilHit
	{
		int16_t		subtune;
		int			score;
		std::string	line;
	};

	auto scoreStil = [ this, &coverOf ] ( const Database::entry* entry )
	{
		std::vector<stilHit>	best;

		hvscDB->visitSTILLines ( entry->lowerFile, [ & ] ( const HVSC_database::stilLine& line )
		{
			auto	score = 0;

			for ( const auto& term : terms )
				if ( term.stil )
				{
					const auto	cover = coverOf ( line.folded, term.text );
					if ( ! cover )
						return true;

					score += cover;
				}

			const auto	known = std::ranges::find ( best, line.subtune, &stilHit::subtune );

			if ( known == best.end () )
				best.push_back ( { line.subtune, score, line.shown } );
			else if ( score > known->score )
				*known = { line.subtune, score, line.shown };

			return true;
		} );

		return best;
	};

	// The picture counts once per word so it outranks the summed cover tiers;
	// the probe comes last so it only runs for rows the text already admitted
	const auto	pictureScore = pictureRank * std::max ( int ( terms.size () ), 1 );

	struct hit
	{
		int						score;
		const Database::entry*	entry;
		int16_t					subtune;
		std::string				stil;
	};

	std::vector<hit>	hits;

	const auto	collect = [ & ] ( const std::vector<const Database::entry*>& entries )
	{
		for ( const auto entry : entries )
		{
			if ( ! matchesFilter ( entry ) )
				continue;

			const auto	score = scoreTerms ( entry );
			if ( ! score )
				continue;

			const auto	pictured = screenshots->hasScreenshots ( entry->lowerFile );
			if ( mustHaveScreenshots && ! pictured )
				continue;

			const auto	total = *score + ( pictured ? pictureScore : 0 );

			if ( ! stilTerms )
			{
				hits.push_back ( { total, entry, 0, {} } );
				continue;
			}

			for ( auto& stil : scoreStil ( entry ) )
				hits.push_back ( { total + stil.score, entry, stil.subtune, std::move ( stil.line ) } );
		}
	};

	collect ( database );
	collect ( userDatabase );

	closeMatch = false;

	if ( hits.empty () && std::ranges::any_of ( terms, [] ( const searchTerm& t ) { return t.text.size () >= shortestFuzzyWord; } ) )
	{
		fuzzy = true;

		collect ( database );
		collect ( userDatabase );

		closeMatch = ! hits.empty ();
	}

	std::ranges::stable_sort ( hits, std::ranges::greater {}, &hit::score );

	rowData.reserve ( hits.size () );
	rowSubtune.reserve ( hits.size () );

	for ( auto& h : hits )
	{
		rowData.push_back ( h.entry );
		rowSubtune.push_back ( h.subtune );

		if ( ! h.stil.empty () )
			stilRows[ { h.entry, h.subtune } ] = std::move ( h.stil );
	}

	unsorted = rowData;
	unsortedSubtune = rowSubtune;
	getHeader ().reSortTable ();

	return int ( rowData.size () );
}
//-----------------------------------------------------------------------------

std::string_view GUI_Results::getRowSubtitle ( const int rowNumber ) const
{
	if ( stilRows.empty () || ! juce::isPositiveAndBelow ( rowNumber, int ( rowData.size () ) ) )
		return {};

	const auto	it = stilRows.find ( { rowData[ rowNumber ], rowSubtune.empty () ? 0 : rowSubtune[ rowNumber ] } );

	return it == stilRows.end () ? std::string_view () : std::string_view ( it->second );
}
//-----------------------------------------------------------------------------

std::vector<db::textSpan> GUI_Results::getHighlights ( const std::string_view shown, const textField field ) const
{
	if ( terms.empty () || shown.empty () )
		return {};

	const auto	stil = field == textField::stil;
	const auto	view = field == textField::name ? &Database::entry::searchName
					 : field == textField::author ? &Database::entry::searchAuthor
					 : field == textField::publisher ? &Database::entry::searchPublisher : nullptr;

	// The shown text folded the way the fields were, each folded character
	// knowing where it came from
	std::string					folded ( shown.size (), '\0' );
	std::vector<db::textSpan>	sources ( shown.size () );

	folded.resize ( db::foldSearch ( shown, folded.data (), sources.data () ) );

	std::vector<db::textSpan>	spans;

	const auto	add = [ & ] ( const textHit hit )
	{
		spans.push_back ( { sources[ hit.pos ].from, sources[ hit.pos + hit.len - 1 ].to } );
	};

	for ( const auto& term : terms )
	{
		if ( term.stil != stil || ( term.field && term.field != view ) )
			continue;

		auto	pos = folded.find ( term.text );

		if ( pos == std::string::npos )
		{
			if ( fuzzy && term.text.size () >= shortestFuzzyWord )
				if ( const auto hit = fuzzyFind ( folded, term.text ) )
					add ( *hit );

			continue;
		}

		for ( ; pos != std::string::npos; pos = folded.find ( term.text, pos + 1 ) )
			add ( { pos, term.text.size () } );
	}

	// Overlapping spans merge so the boxes never stack
	std::ranges::sort ( spans, {}, &db::textSpan::from );

	std::vector<db::textSpan>	merged;

	for ( const auto& span : spans )
		if ( ! merged.empty () && span.from <= merged.back ().to )
			merged.back ().to = std::max ( merged.back ().to, span.to );
		else
			merged.push_back ( span );

	return merged;
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
	pages.loadTune ( juce::String ( file.data (), file.size () ), rowSubtune.empty () ? 0 : rowSubtune[ lastRowSelected ], "search", -1 );
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

	// Tags need the bare key
	juce::StringArray	tuneKeys;
	for ( auto i = 0; i < rows.size (); ++i )
		if ( const auto ent = rowData[ rows[ i ] ] )
			tuneKeys.addIfNotAlreadyThere ( juce::String ( ent->file.data (), ent->file.size () ) + "," + juce::String ( rowSubtune.empty () ? 0 : rowSubtune[ rows[ i ] ] ) );

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

