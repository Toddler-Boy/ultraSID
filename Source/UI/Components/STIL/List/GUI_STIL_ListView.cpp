#include "GUI_STIL_ListView.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Helpers/Regex.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Config/Preferences.h"
#include "Data/Likes.h"
#include "Database/Database.h"
#include "Database/HVSCDatabase.h"
#include "Database/TuneInfo.h"
#include "Helpers/Messages.h"
#include "Resources/STIL_Lookup.h"
#include "UI/GUI_AppLookAndFeel.h"
#include "UI/ui-colors.h"
#include "UI/UI_Menus.h"

//----------------------------------------------------------------------------------

constexpr auto	STIL_name_width = 194;
constexpr auto	STIL_author_width = 110;

GUI_STIL_ListView::GUI_STIL_ListView ()
	: hover ( *this ), smoothScroll ( *this )
{
	setName ( "stilListView" );

	hover.addChangeListener ( this );

	setModel ( this );
	setMultipleSelectionEnabled ( true );

	setOutlineThickness ( 0 );
	setRowHeight ( 22 );

	setHeaderHeight ( 28 );

	setAutoSizeMenuOptionShown ( false );

	{
		auto&	header = getHeader ();

		constexpr auto	flags = juce::TableHeaderComponent::visible;
		header.addColumn ( "", columnId::animation, 20, 20, 20, flags );
		header.addColumn ( "#", columnId::tuneNo, 30, 30, 30, flags );
		header.addColumn ( "Title", columnId::name, STIL_name_width, STIL_name_width, STIL_name_width + STIL_author_width, flags );
		header.addColumn ( "Artist", columnId::author, STIL_author_width, STIL_author_width, STIL_author_width, flags );
		header.addColumn ( "Time", columnId::length, 40, 40, 40, flags );
		header.addColumn ( "", columnId::liked, 20, 20, 20, flags );

		auto&	props = header.getProperties ();

		props.set ( "line", 6.0f );

		props.set ( "colJust" + juce::String ( columnId::tuneNo ), int ( juce::Justification::Flags::centred ) );
		props.set ( "colOff" + juce::String ( columnId::tuneNo ), 0.0f );

		props.set ( "colJust" + juce::String ( columnId::length ), int ( juce::Justification::Flags::centredRight ) );
	}

	getViewport ()->setScrollBarsShown ( true, false );
	getViewport ()->getVerticalScrollBar ().setAutoHide ( false );

	// A tab stop only while it has rows, see layout ()
	setWantsKeyboardFocus ( false );
	getProperties ().set ( "focusMargin", "2" );

	layout ();
}
//-----------------------------------------------------------------------------

GUI_STIL_ListView::~GUI_STIL_ListView ()
{
	hover.removeChangeListener ( this );
}
//-----------------------------------------------------------------------------

int GUI_STIL_ListView::getNumRows ()
{
	return int ( rowData.size () );
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::cellClicked ( int row, int column, const juce::MouseEvent& e )
{
	if ( column == columnId::liked )
	{
		const juce::SharedResourcePointer<Likes>	likes;

		const auto	subtune = rowData[ row ]->no + 1;
		const auto	stdTuneName = tuneName.toStdString ();;
		likes->toggle ( stdTuneName, subtune );

		msg::LikeChanged { stdTuneName, subtune }.send ();

		return;
	}

	if ( ! e.mods.isPopupMenu () )
		return;

	auto	m = UI::newPopupMenu ( *this );

	// Add to playlist
	const auto	rows = getSelectedRows ();

	juce::StringArray	selectedTunes;
	for ( auto i = 0; i < rows.size (); ++i )
		selectedTunes.add ( tuneName + "," + juce::String ( rowData[ rows[ i ] ]->no + 1 ) );

	UI::menu_AddToPlaylist ( m, selectedTunes );

	// Export track
	m.addSeparator ();
	UI::menu_ExportTrack ( m, selectedTunes );

	UI::showMenuAtMouse ( m, *this );
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::cellDoubleClicked ( int row, int /*column*/, const juce::MouseEvent& )
{
	returnKeyPressed ( row );
}
//-----------------------------------------------------------------------------

bool GUI_STIL_ListView::keyPressed ( const juce::KeyPress& key )
{
	if ( key.getModifiers ().isCommandDown () )
		return false;

	return juce::TableListBox::keyPressed ( key );
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::returnKeyPressed ( int lastRowSelected )
{
	const auto	subtune = rowData[ lastRowSelected ]->no;

	if ( tunePlaying != subtune )
	{
		// Repaint the playing row
		const auto	oldPlaying = tunePlaying;
		tunePlaying = -1;

		if ( oldPlaying >= 0 && oldPlaying < int ( rowMap.size () ) )
			if ( const auto rowNo = rowMap[ oldPlaying ]; rowNo >= 0 )
				repaintRow ( rowNo );
	}

	// Trigger playback
	msg::PlaySubtune { subtune + 1 }.send ();
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::paintRowBackground ( juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	if ( rowIsSelected )
		g.setColour ( UI::getShade ( UI::shades::selected ) );
	else if ( rowNumber == hoverPosition )
		g.setColour ( UI::getShade ( UI::shades::hover ) );
	else
		return;

	const auto	b = juce::Rectangle<int> ( width, height ).toFloat ();

	g.fillRoundedRectangle ( b, UI::corner ( UI::corners::browser_list_row, b ) );
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/ )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	g.setFont ( UI::font ( UI::fonts::stil_list ) );

	auto	b = juce::Rectangle<int> ( width, height ).toFloat ().reduced ( 4.0f, 0.0f );

	const auto& ent = *rowData[ rowNumber ];

	const auto	isPlaying = tunePlaying == ent.no;
	const auto	color = findColour ( isPlaying && columnId <= columnId::author ? UI::colors::text : UI::colors::textMuted );

	switch ( columnId )
	{
		case columnId::animation:
			if ( isPlaying )
				GUI_AppLookAndFeel::drawPlaybackAnimation ( g, b.withSizeKeepingCentre ( 15.0f, 12.0f ).translated ( 4.0f, 1.0f ), findColour ( UI::accentBright ), animState );
			break;

		case columnId::tuneNo:
			g.setColour ( color );
			g.drawText ( juce::String ( ent.no + 1 ), b, juce::Justification::centred, false );
			break;

		case columnId::name:
			{
				// Icon; the padding's right side is the gap to the text
				{
					const auto	pad = UI::paddingDef ( UI::paddings::stil_list_icon );
					const auto	iconHeight = b.getHeight () * 0.56f;

					const auto	r = b.removeFromLeft ( pad.left + iconHeight + pad.right )
									 .withTrimmedLeft ( pad.left ).withWidth ( iconHeight )
									 .withTrimmedTop ( pad.top ).withTrimmedBottom ( pad.bottom );

					const auto&	p = UI::getScaledPath ( icons->get ( "stil/list/" + ent.categoryStr ), r );

					g.setColour ( color.withMultipliedAlpha ( 0.5f ) );
					g.fillPath ( p );
				}

				// Per-subtune scan dots (filter, digi, one-shot, delayed
				// start) in the sid_scanner's colors; the search list shows
				// the same per tune, this shows which subtune carries what
				if ( buildinfo::isDeveloperMode () )
				{
					const juce::SharedResourcePointer<Database>			database;
					const juce::SharedResourcePointer<HVSC_database>	hvscDB;

					if ( const auto dbEnt = database->findEntry ( tuneName.toStdString () ) )
					{
						constexpr auto	dotSize = 6.0f;
						constexpr auto	dotGap = 2.0f;

						auto	dots = b.removeFromRight ( 4.0f * dotSize + 3.0f * dotGap ).withSizeKeepingCentre ( 4.0f * dotSize + 3.0f * dotGap, dotSize );

						auto drawDot = [ &g, &dots ] ( const bool on, const juce::Colour col )
						{
							const auto	r = dots.removeFromLeft ( dotSize );
							dots.removeFromLeft ( dotGap );

							g.setColour ( col.withMultipliedAlpha ( on ? 1.0f : 0.1f ) );
							g.fillEllipse ( r );
						};

						drawDot ( dbEnt->hasFilter ( ent.no ), findColour ( UI::colors::filterOn ) );
						drawDot ( dbEnt->hasDigi ( ent.no ), findColour ( UI::colors::digi ).withRotatedHue ( -0.2f ) );
						drawDot ( dbEnt->hasOneShot ( ent.no ), juce::Colours::red );
						drawDot ( hvscDB->getStartMs ( tuneName.toStdString (), ent.no + 1 ) > 0, juce::Colours::yellow );

						b.removeFromRight ( 4.0f );
					}
				}

				const auto	name = ent.timeStr.isEmpty () ? ( "Tune " + juce::String ( ent.no + 1 ) ) : ent.tuneName.unquoted ();
				g.setColour ( color );
				g.drawText ( name, b, juce::Justification::centredLeft, true );
			}
			break;

		case columnId::author:
			g.setColour ( color );
			g.drawText ( ent.authorName, b, juce::Justification::centredLeft, true );
			break;

		case columnId::length:
			g.setColour ( color );
			if ( ent.timeStr.isEmpty () )
			{
				const juce::SharedResourcePointer<Preferences>	preferences;

				const auto	lenMS = preferences->getClamped ( "songs/unknown" ) * 60 * 1000;
				g.drawText ( SID::convertTimeToString ( lenMS ), b, juce::Justification::centredRight, false );
			}
			else
			{
				g.drawText ( ent.timeStr, b, juce::Justification::centredRight, false );
			}
			break;

		case columnId::liked:
			{
				const juce::SharedResourcePointer<Likes>	likes;

				const auto	liked = likes->isLiked ( tuneName.toStdString (), ent.no + 1 );
				const auto& p = UI::getScaledPath ( icons->get ( liked ? "liked" : "not-liked" ), b, 0, 0.1f );

				g.setColour ( findColour ( liked ? UI::colors::tagLiked : UI::colors::textMuted ).withMultipliedAlpha ( liked ? 1.0f : 0.5f ) );
				g.fillPath ( p );
			}
			break;
	}
}
//-----------------------------------------------------------------------------

juce::var GUI_STIL_ListView::getDragSourceDescription ( const juce::SparseSet<int>& rowsToDescribe )
{
	juce::Array<juce::var>	subtunes;

	for ( auto i = 0; i < rowsToDescribe.size (); ++i )
		subtunes.add ( rowData[ rowsToDescribe[ i ] ]->no + 1 );

	auto*	desc = new juce::DynamicObject ();
	desc->setProperty ( "source", "STIL" );
	desc->setProperty ( "tune", tuneName );
	desc->setProperty ( "subtunes", subtunes );

	return desc;
}
//-----------------------------------------------------------------------------

juce::String GUI_STIL_ListView::getCellTooltip ( int rowNumber, int columnId )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return {};

	if ( columnId == columnId::name )	return rowData[ rowNumber ]->tuneName;
	if ( columnId == columnId::author )	return rowData[ rowNumber ]->authorName;

	return {};
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::layout ()
{
	const juce::SharedResourcePointer<Preferences>	preferences;

	const auto	onlyStingers = onlyHasStingers ();
	const auto	showStingers = ( ! preferences->get<bool> ( "stil/show-tunes-only" ) ) || onlyStingers;

	rowData.clear ();
	rowMap.resize ( sourceData.size () );
	std::ranges::fill ( rowMap, -1 );

	auto	authorUsed = false;

	for ( auto rowNo = 0; const auto& row : sourceData )
	{
		if ( row.songFlag || showStingers || rowNo == tunePlaying || ( rowNo + 1 ) == mainTuneNo )
		{
			rowMap[ rowNo ] = int ( rowData.size () );
			rowData.emplace_back ( &row );
			if ( row.authorName.isNotEmpty () )
				authorUsed = true;
		}

		++rowNo;
	}

	getHeader ().setColumnVisible ( columnId::author, authorUsed );

	auto	nameWidth = STIL_name_width;
	if ( ! authorUsed )		nameWidth += STIL_author_width;

	getHeader ().setColumnWidth ( columnId::name, nameWidth );

	updateContent ();
	repaint ();

	// A tab stop only while it has rows
	setWantsKeyboardFocus ( ! rowData.empty () );

	if ( tunePlaying < 0 || tunePlaying >= int ( rowMap.size () ) )
		return;

	if ( const auto rowNo = rowMap[ tunePlaying ]; rowNo >= 0 )
		selectRow ( rowNo );
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::setTune ( const juce::String& name, const int _mainTuneNo )
{
	tuneName = name;
	mainTuneNo = _mainTuneNo;
}
//-----------------------------------------------------------------------------

void GUI_STIL_ListView::setBlocks ( const GUI_STIL_blocks& blocks )
{
	sourceData.clear ();

	for ( tuneEntry* item = nullptr; const auto& [ name, value, _ ] : blocks )
	{
		if ( name == "TUNE" )
		{
			sourceData.push_back ( {} );
			item = &sourceData.back ();
			item->no = value.getIntValue () - 1;
		}
		else if ( item && ( name == "NAME" || name == "TITLE" ) )
		{
			if ( item->tuneName.isEmpty () )
			{
				auto	str = value.toStdString ();

				// Remove trailing "(xx:xx-yy:yy)" from the name
				str = regex::Pattern ( "\\s*\\([0-9\\:\\-\\s]+\\)$" ).replaceAll ( str, "" );

				// Remove trailing "[.+]" from the name
				str = regex::Pattern ( "\\s*\\[.+\\]$" ).replaceAll ( str, "" );

				item->tuneName = str;
			}
		}
		else if ( item && name == "AUTHOR" )
		{
			if ( item->authorName.isEmpty () )
				item->authorName = value;
		}
	}
}
//----------------------------------------------------------------------------------

void GUI_STIL_ListView::setTunePlaying ( const int tune )
{
	repaint ();

	tunePlaying = tune - 1;

	if ( tunePlaying < 0 || tunePlaying >= int ( rowMap.size () ) )
		return;

	if ( const auto rowNo = rowMap[ tunePlaying ]; rowNo >= 0 )
		selectRow ( rowNo );
}
//----------------------------------------------------------------------------------

void GUI_STIL_ListView::setDefaultTune ( const std::string& title, const int tune )
{
	if ( tune < 1 || tune > int ( sourceData.size () ) )
		return;

	auto&	item = sourceData[ tune - 1 ];

	if ( item.tuneName.startsWith ( "Tune " ) || item.tuneName.startsWith ( "Stinger " ) || item.tuneName.startsWith ( "FX " ) )
		item.tuneName = title;

	layout ();
}
//----------------------------------------------------------------------------------

void GUI_STIL_ListView::setTuneLength ( const int tune, int lengthMs )
{
	if ( tune < 1 || tune > int ( sourceData.size () ) )
		return;

	auto&	item = sourceData[ tune - 1 ];

	item.lengthMS = lengthMs;
	item.songFlag = SID::isSong ( lengthMs );

	item.timeStr = SID::convertTimeToString ( lengthMs );

	// Sound FX with names are usually Stingers
	if ( SID::isFX ( lengthMs ) && item.tuneName.isNotEmpty () )
		lengthMs = SID::stingerMs;

	auto	nameSub = juce::String ( "Tune" );

	item.categoryStr = "song";
	if ( SID::isFX ( lengthMs ) )
	{
		item.categoryStr = "fx";
		nameSub = "FX";
	}
	else if ( SID::isStinger ( lengthMs ) )
	{
		item.categoryStr = "stinger";
		nameSub = "Stinger";
	}

	if ( item.tuneName.isEmpty () )
		item.tuneName = nameSub + " " + juce::String ( item.no + 1 );
	else if ( item.tuneName.startsWithChar ( '"' ) && item.tuneName.endsWithChar ( '"' ) )
		item.categoryStr = "speech";
}
//----------------------------------------------------------------------------------

void GUI_STIL_ListView::timerUpdate ( const float secondsPassed )
{
	if ( tunePlaying < 0 || tunePlaying >= int ( rowMap.size () ) )
		return;

	animState += secondsPassed * 2.0f;

	if ( const auto rowNo = rowMap[ tunePlaying ]; rowNo >= 0 )
		UI::repaintCell ( this, rowNo, columnId::animation );
}
//-----------------------------------------------------------------------------

int GUI_STIL_ListView::getMaximumHeight ()
{
	return	getHeader ().getHeight ()
			+ getOutlineThickness ()
			+ ( getNumRows () * getRowHeight () );
}
//-----------------------------------------------------------------------------

bool GUI_STIL_ListView::hasStingers () const
{
	if ( sourceData.size () < 2 )
		return false;

	// Find stingers and FX
	return std::ranges::any_of ( sourceData, [] ( const auto& row ) { return ! row.songFlag; } );
}
//----------------------------------------------------------------------------------

bool GUI_STIL_ListView::hasSongs () const
{
	return std::ranges::any_of ( sourceData, [] ( const auto& row ) { return row.songFlag; } );
}
//----------------------------------------------------------------------------------

bool GUI_STIL_ListView::onlyHasStingers () const
{
	return std::ranges::all_of ( sourceData, [] ( const auto& row ) { return ! row.songFlag; } );
}
//----------------------------------------------------------------------------------

bool GUI_STIL_ListView::onlyHasSongs () const
{
	return std::ranges::all_of ( sourceData, [] ( const auto& row ) { return row.songFlag; } );
}
//----------------------------------------------------------------------------------

void GUI_STIL_ListView::changeListenerCallback ( juce::ChangeBroadcaster* source )
{
	if ( source != &hover )
		return;

	const auto	oldHover = hoverPosition;
	hoverPosition = hover.getHoverPos ();
	if ( oldHover >= 0 )
		repaintRow ( oldHover );

	repaintRow ( hoverPosition );
}
//-----------------------------------------------------------------------------
