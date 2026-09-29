#include <JuceHeader.h>

#include "GUI_ScreenshotBrowser.h"

#include "ultra-shared/Helpers/ImageUtils.h"
#include "ultra-shared/Helpers/TextUtils.h"
#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"
#include "ultra-shared/Video/VIC2_Render.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

namespace
{
	constexpr auto	rowHeight = 56;

	// Thumbnails keep the CRT picture's aspect
	constexpr auto	thumbRatio = ( 320.0f * VIC2::truePalX ) / 200.0f;

	[[ nodiscard ]] juce::String leafName ( const std::string& path )
	{
		return juce::String ( path ).fromLastOccurrenceOf ( "/", false, false );
	}

	[[ nodiscard ]] juce::String parentFolder ( const juce::String& path )
	{
		return path.containsChar ( '/' ) ? path.upToLastOccurrenceOf ( "/", false, false ) : juce::String ();
	}
}
//-----------------------------------------------------------------------------

GUI_ScreenshotBrowser::GUI_ScreenshotBrowser ()
{
	setName ( "browser" );

	// The CRT page's crt.json places them by these names
	pathBar.setName ( "pathBar" );
	searchInfo.setName ( "info" );
	list.setName ( "list" );

	addAndMakeVisible ( pathBar );
	addAndMakeVisible ( searchBar );
	addAndMakeVisible ( searchInfo );
	addAndMakeVisible ( list );

	pathBar.onNavigate = [ this ] ( const juce::String& f )	{	navigateTo ( f );	};

	searchBar.onTextChange = [ this ]
	{
		query = searchBar.getTextEditor ().getText ().trim ();

		relist ();

		list.deselectAllRows ();
		list.updateContent ();
		list.getViewport ()->setViewPosition ( 0, 0 );
		list.repaint ();
	};

	searchBar.getTextEditor ().onEscapePressed = [ this ]
	{
		clearSearch ();
		list.grabKeyboardFocus ();
	};

	searchBar.getTextEditor ().setTitle ( strings->get ( "crt-browser/search" ) );

	searchBar.getTextEditor ().onReturnPressed = [ this ]
	{
		if ( entries.files.empty () )
			return;

		list.selectRow ( int ( entries.folders.size () ) );
		list.grabKeyboardFocus ();
	};

	list.setModel ( this );
	list.setRowHeight ( rowHeight );
	list.setOutlineThickness ( 0 );
	list.setMultipleSelectionEnabled ( false );
	list.setColour ( juce::ListBox::backgroundColourId, juce::Colours::transparentBlack );
	list.getViewport ()->setScrollBarsShown ( true, false );

	list.extraKey = [ this ] ( const juce::KeyPress& key )
	{
		if ( key.isKeyCode ( juce::KeyPress::backspaceKey ) && folder.isNotEmpty () )
		{
			navigateTo ( parentFolder ( folder ) );
			return true;
		}

		if ( key.isKeyCode ( juce::KeyPress::deleteKey ) )
		{
			if ( const auto row = list.getSelectedRow (); juce::isPositiveAndBelow ( row, getNumRows () ) && ! isFolderRow ( row ) && onDelete )
				onDelete ();

			return true;
		}

		return false;
	};

	lookAndFeelChanged ();
	navigateTo ( {} );
}
//-----------------------------------------------------------------------------

GUI_ScreenshotBrowser::~GUI_ScreenshotBrowser ()
{
	list.setModel ( nullptr );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::paint ( juce::Graphics& g )
{
	constexpr auto	blend = 0.067f;

	const auto	b = getLocalBounds ().toFloat ();

	g.setColour ( UI::getShade ( blend ) );
	GUI_LookAndFeel::drawOutlinedRect ( g, b, UI::corner ( UI::corners::settings_box, b ), UI::lineWidth ( UI::lines::settings_box ), UI::getShade ( blend * 2.0f ) );
}
//-----------------------------------------------------------------------------


void GUI_ScreenshotBrowser::lookAndFeelChanged ()
{
	auto&	editor = searchBar.getTextEditor ();

	const auto	txtCol = findColour ( UI::colors::text );
	editor.setTextToShowWhenEmpty ( strings->get ( "crt-browser/search" ), txtCol.withMultipliedAlpha ( 0.25f ) );
	editor.applyColourToAllText ( txtCol );

	repaint ();
}
//-----------------------------------------------------------------------------

juce::Drawable* GUI_ScreenshotBrowser::icon ( const juce::String& key )
{
	const auto	col = findColour ( UI::colors::text );

	if ( col != iconColor )
	{
		for ( auto& [ _, d ] : iconCache )
			d->replaceColour ( iconColor, col );

		iconColor = col;
	}

	auto&	d = iconCache[ key ];
	if ( ! d )
	{
		d = UI::getSVG ( icons->get ( key ) ).first;
		d->replaceColour ( juce::Colours::black, iconColor );
	}

	return d.get ();
}
//-----------------------------------------------------------------------------

int GUI_ScreenshotBrowser::getNumRows ()
{
	return int ( entries.folders.size () + entries.files.size () );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::paintListBoxItem ( int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	auto	b = juce::Rectangle<int> ( width, height ).toFloat ();

	if ( rowIsSelected )
	{
		g.setColour ( UI::getShade ( UI::shades::selected ) );
		g.fillRoundedRectangle ( b, UI::corner ( UI::corners::browser_list_row, b ) );
	}

	b.reduce ( 4.0f, 4.0f );

	g.setFont ( UI::font ( UI::fonts::browser_text ) );
	g.setColour ( findColour ( UI::colors::text ) );

	if ( isFolderRow ( rowNumber ) )
	{
		const auto&	name = entries.folders[ size_t ( rowNumber ) ];

		const auto	iconArea = b.removeFromLeft ( b.getHeight () * thumbRatio ).reduced ( 10.0f );
		icon ( "crt/browser_folder" )->drawWithin ( g, iconArea, juce::RectanglePlacement::centred, 1.0f );

		b.removeFromLeft ( 6.0f );
		g.drawText ( leafName ( name ), b, juce::Justification::centredLeft, true );
		return;
	}

	const auto&	art = entries.files[ size_t ( rowNumber ) - entries.folders.size () ];

	{
		const auto	r = b.removeFromLeft ( b.getHeight () * thumbRatio );

		auto&	img = thumbnailCache->getArtThumbnail ( art, isNTSC, [ safe = juce::Component::SafePointer<GUI_ScreenshotBrowser> ( this ), rowNumber ]
		{
			if ( safe != nullptr )
				safe->list.repaintRow ( rowNumber );
		} );

		const auto	gs = GUI_RoundedClip ( g, r, UI::corner ( UI::corners::browser_thumbnail, r ) );
		g.setOpacity ( 1.0f );
		img.draw ( g, r );
	}

	b.removeFromLeft ( 6.0f );

	// The user's own files carry the user mark
	if ( lookup->isUserFile ( art ) )
	{
		const auto	mark = b.removeFromRight ( 16.0f ).withSizeKeepingCentre ( 12.0f, 12.0f );
		icon ( "crt-user" )->drawWithin ( g, mark, juce::RectanglePlacement::centred, 0.6f );
		b.removeFromRight ( 4.0f );
	}

	const auto	hint = imageutils::hintFromFilename ( art );

	// Name over the file's hints; a search result first names its subfolder
	juce::StringArray	hints;

	if ( hint.forceNTSC )
		hints.add ( strings->get ( "crt-browser/hint_ntsc" ) );

	if ( query.isNotEmpty () )
	{
		const auto	below = parentFolder ( art ).substring ( folder.isEmpty () ? 0 : folder.length () + 1 );
		if ( below.isNotEmpty () )
			hints.add ( below );
	}

	if ( const auto flags = thumbnailCache->getPictureFlags ( art ) )
	{
		hints.add ( strings->get ( *flags & pictureanalyzer::multicolor ? "crt-browser/hint_multicolor" : "crt-browser/hint_hires" ) );

		if ( *flags & pictureanalyzer::borderSprites )
			hints.add ( strings->get ( "crt-browser/hint_border_sprites" ) );

		if ( *flags & pictureanalyzer::rasterSplits )
			hints.add ( strings->get ( "crt-browser/hint_raster_splits" ) );
	}

	if ( thumbnailCache->isInterlaced ( art ) )
		hints.add ( strings->get ( "crt-browser/hint_interlaced" ) );

	if ( hint.firstLuma )
		hints.add ( strings->get ( "crt-browser/hint_first_luma" ) );

	b.reduce ( 0.0f, 4.0f );

	g.drawText ( leafName ( hint.name.toStdString () ), b.removeFromTop ( b.getHeight () / 2.0f ), juce::Justification::centredLeft, true );

	g.setFont ( UI::font ( UI::fonts::browser_small ) );
	g.setColour ( findColour ( UI::colors::textMuted ) );
	g.drawText ( hints.joinIntoString ( ", " ), b, juce::Justification::centredLeft, true );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::openRow ( const int row )
{
	if ( isFolderRow ( row ) )
		navigateTo ( entries.folders[ size_t ( row ) ] );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::listBoxItemClicked ( int row, const juce::MouseEvent& e )
{
	if ( ! e.mods.isPopupMenu () || ! juce::isPositiveAndBelow ( row, getNumRows () ) || isFolderRow ( row ) )
		return;

	list.selectRow ( row );

	if ( onMenu )
		onMenu ();
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::listBoxItemDoubleClicked ( int row, const juce::MouseEvent& )
{
	openRow ( row );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::returnKeyPressed ( int row )
{
	openRow ( row );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::selectedRowsChanged ( int lastRowSelected )
{
	if ( ! juce::isPositiveAndBelow ( lastRowSelected, getNumRows () ) || isFolderRow ( lastRowSelected ) )
		return;

	if ( onPick )
		onPick ( entries.files[ size_t ( lastRowSelected ) - entries.folders.size () ] );
}
//-----------------------------------------------------------------------------

juce::String GUI_ScreenshotBrowser::getNameForRow ( int rowNumber )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return {};

	if ( isFolderRow ( rowNumber ) )
		return leafName ( entries.folders[ size_t ( rowNumber ) ] );

	return leafName ( imageutils::hintFromFilename ( entries.files[ size_t ( rowNumber ) - entries.folders.size () ] ).name.toStdString () );
}
//-----------------------------------------------------------------------------

bool GUI_ScreenshotBrowser::isInterestedInFileDrag ( const juce::StringArray& files )
{
	return textutils::getFilteredStrings ( files, { ".png" } ).size () > 0;
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::filesDropped ( const juce::StringArray& files, int, int )
{
	const auto	pictures = textutils::getFilteredStrings ( files, { ".png" } );

	if ( pictures.size () && onDropFiles )
		onDropFiles ( pictures, folder );
}
//-----------------------------------------------------------------------------

GUI_ScreenshotBrowser::state GUI_ScreenshotBrowser::getState () const
{
	const auto	row = list.getSelectedRow ();
	const auto	picture = juce::isPositiveAndBelow ( row, int ( entries.folders.size () + entries.files.size () ) ) && ! isFolderRow ( row )
							? juce::String ( entries.files[ size_t ( row ) - entries.folders.size () ] ) : juce::String ();

	return { folder, query, picture, pendingScroll.value_or ( list.getVerticalPosition () ) };
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::setState ( const state& s )
{
	navigateTo ( s.folder );

	// The editor reports text changes later, the results must be listed now
	searchBar.getTextEditor ().setText ( s.query, false );
	searchBar.updateClearButton ();

	query = s.query.trim ();
	relist ();
	list.updateContent ();

	if ( const auto row = rowOfPicture ( s.picture ); row >= 0 )
		selectQuietly ( row );

	pendingScroll = s.scroll;
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::applyPendingScroll ()
{
	if ( pendingScroll )
		list.setVerticalPosition ( *pendingScroll );

	pendingScroll.reset ();
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::relist ()
{
	entries = query.isEmpty () ? lookup->list ( folder.toStdString () ) : lookup->search ( folder.toStdString (), query );

	const auto	count = int ( entries.files.size () );
	const auto	number = textutils::getHumanNumber ( count );

	if ( query.isEmpty () )
		searchInfo.setText ( strings->get ( count == 1 ? "crt-browser/picture" : "crt-browser/pictures" ).replace ( "{}", number ) );
	else if ( count > 0 )
		searchInfo.setText ( strings->get ( count == 1 ? "search/result" : "search/results" ).replace ( "{}", number ) );
	else
		searchInfo.setText ( strings->get ( "search/no_results" ) );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::clearSearch ()
{
	searchBar.getTextEditor ().setText ( {}, true );
	searchBar.updateClearButton ();
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::navigateTo ( const juce::String& f )
{
	folder = f;

	query.clear ();
	searchBar.getTextEditor ().setText ( {}, false );
	searchBar.updateClearButton ();

	relist ();

	pathBar.setPath ( strings->get ( "crt-browser/root" ), folder );

	list.deselectAllRows ();
	list.updateContent ();
	list.getViewport ()->setViewPosition ( 0, 0 );
	list.repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::refresh ()
{
	const auto	selected = list.getSelectedRow ();
	const auto	selectedName = juce::isPositiveAndBelow ( selected, getNumRows () ) && ! isFolderRow ( selected )
									? juce::String ( entries.files[ size_t ( selected ) - entries.folders.size () ] ) : juce::String ();
	const auto	fileCount = entries.files.size ();

	relist ();

	// The folder itself went away
	if ( entries.folders.empty () && entries.files.empty () && folder.isNotEmpty () && query.isEmpty () )
	{
		navigateTo ( parentFolder ( folder ) );
		return;
	}

	pathBar.setPath ( strings->get ( "crt-browser/root" ), folder );

	colorNames.clear ();
	colorNames.addTokens ( strings->get ( "crt-browser/vic2_colors" ), ",", "" );
	colorNames.trim ();

	list.updateContent ();

	// A deleted picture: the next one moved into its row (or the new last one) gets picked
	const auto	deleted = selectedName.isNotEmpty () && rowOfPicture ( selectedName ) < 0 && entries.files.size () < fileCount;

	if ( const auto row = rowOfPicture ( selectedName ); row >= 0 )
		selectQuietly ( row );
	else if ( deleted && ! entries.files.empty () )
	{
		// Usually the same row index: the list sees no change and would not pick
		const auto	row = std::min ( selected, getNumRows () - 1 );
		selectQuietly ( row );

		if ( onPick )
			onPick ( entries.files[ size_t ( row ) - entries.folders.size () ] );
	}
	else
		list.deselectAllRows ();

	list.repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::selectQuietly ( const int row )
{
	const auto	pick = std::move ( onPick );
	onPick = nullptr;

	list.selectRow ( row );

	onPick = std::move ( pick );
}
//-----------------------------------------------------------------------------

int GUI_ScreenshotBrowser::rowOfPicture ( const juce::String& artName ) const
{
	if ( artName.isEmpty () )
		return -1;

	const auto	it = std::ranges::find ( entries.files, artName.toStdString () );
	if ( it == entries.files.end () )
		return -1;

	return int ( entries.folders.size () + size_t ( it - entries.files.begin () ) );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::selectPicture ( const juce::String& artName )
{
	if ( artName.isEmpty () )
		return;

	if ( const auto f = parentFolder ( artName ); ! f.equalsIgnoreCase ( folder ) )
		navigateTo ( f );

	const auto	row = rowOfPicture ( artName );
	if ( row < 0 || row == list.getSelectedRow () )
		return;

	selectQuietly ( row );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::highlightPicture ( const juce::String& artName )
{
	if ( const auto row = rowOfPicture ( artName ); row >= 0 )
		selectQuietly ( row );
	else
		list.deselectAllRows ();
}
//-----------------------------------------------------------------------------

bool GUI_ScreenshotBrowser::List::keyPressed ( const juce::KeyPress& key )
{
	if ( extraKey && extraKey ( key ) )
		return true;

	return juce::ListBox::keyPressed ( key );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::PathBar::setPath ( const juce::String& rootName, const juce::String& f )
{
	folder = f;

	crumbs.clear ();
	crumbs.add ( rootName );
	crumbs.addTokens ( folder, "/", "" );
	crumbs.removeEmptyStrings ();

	hoverCrumb = -1;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::PathBar::paint ( juce::Graphics& g )
{
	const auto	font = UI::font ( UI::fonts::browser_text );
	const auto	separator = juce::String ( " / " );

	g.setFont ( font );

	const auto	textCol = findColour ( UI::colors::text );
	const auto	mutedCol = findColour ( UI::colors::textMuted );

	auto	b = getLocalBounds ().toFloat ();
	crumbBounds.clear ();

	// The tail always fits: drop leading crumbs until it does
	auto	first = 0;
	{
		auto widthFrom = [ & ] ( const int start )
		{
			auto	w = 0.0f;
			for ( auto i = start; i < crumbs.size (); ++i )
				w += juce::GlyphArrangement::getStringWidth ( font, crumbs[ i ] ) + ( i > start ? juce::GlyphArrangement::getStringWidth ( font, separator ) : 0.0f );
			return w;
		};

		while ( first < crumbs.size () - 1 && widthFrom ( first ) > b.getWidth () )
			++first;
	}

	for ( auto i = 0; i < crumbs.size (); ++i )
	{
		if ( i < first )
		{
			crumbBounds.emplace_back ();
			continue;
		}

		if ( i > first )
		{
			const auto	sepW = juce::GlyphArrangement::getStringWidth ( font, separator );
			g.setColour ( mutedCol );
			g.drawText ( separator, b.removeFromLeft ( sepW ), juce::Justification::centredLeft, false );
		}

		const auto	w = juce::GlyphArrangement::getStringWidth ( font, crumbs[ i ] );
		const auto	r = b.removeFromLeft ( std::min ( w, b.getWidth () ) );

		const auto	last = i == crumbs.size () - 1;
		g.setColour ( last || i == hoverCrumb ? textCol : mutedCol );
		g.drawText ( crumbs[ i ], r, juce::Justification::centredLeft, true );

		crumbBounds.emplace_back ( r );
	}
}
//-----------------------------------------------------------------------------

int GUI_ScreenshotBrowser::PathBar::crumbAt ( const juce::Point<float> p ) const
{
	for ( auto i = 0; i < int ( crumbBounds.size () ); ++i )
		if ( ! crumbBounds[ size_t ( i ) ].isEmpty () && crumbBounds[ size_t ( i ) ].contains ( p ) )
			return i;

	return -1;
}
//-----------------------------------------------------------------------------

juce::String GUI_ScreenshotBrowser::PathBar::folderOfCrumb ( const int index ) const
{
	juce::StringArray	parts;
	for ( auto i = 1; i <= index && i < crumbs.size (); ++i )
		parts.add ( crumbs[ i ] );

	return parts.joinIntoString ( "/" );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::PathBar::mouseDown ( const juce::MouseEvent& e )
{
	// The last crumb is the folder already shown
	if ( const auto i = crumbAt ( e.position ); i >= 0 && i < crumbs.size () - 1 && onNavigate )
		onNavigate ( folderOfCrumb ( i ) );
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::PathBar::mouseMove ( const juce::MouseEvent& e )
{
	const auto	i = crumbAt ( e.position );
	if ( i == hoverCrumb )
		return;

	hoverCrumb = i;
	setMouseCursor ( i >= 0 && i < crumbs.size () - 1 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor );
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ScreenshotBrowser::PathBar::mouseExit ( const juce::MouseEvent& )
{
	hoverCrumb = -1;
	repaint ();
}
//-----------------------------------------------------------------------------
