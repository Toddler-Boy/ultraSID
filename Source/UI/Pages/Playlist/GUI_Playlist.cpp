#include <JuceHeader.h>

#include "GUI_Playlist.h"

#include "ultra-shared/Helpers/TextUtils.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/UndoManager.h"
#include "Data/Playlists.h"
#include "Database/TuneInfo.h"
#include "Helpers/Messages.h"
#include "UI/UI_Menus.h"

#include "../GUI_Pages.h"


//-----------------------------------------------------------------------------

constexpr auto	gradientSize = 400;
constexpr auto	bentoGap = 8;

GUI_Playlist::GUI_Playlist ( GUI_Pages& _pages )
	: pages ( _pages )
{
	setName ( "playlist" );

	info.setName ( "info" );

	// Back button
	{
		backButton.tooltips = { "playlist/back" };
		backButton.margin = 6.0f;
		backButton.translation.x = -1.0f;
		backButton.bckAlpha[ 0 ] = 0.2f;
		backButton.bckAlpha[ 1 ] = 0.4f;
		backButton.getProperties ().set ( "focusRadius", 1000 );

		backButton.onClick = []
		{
			msg::ShowPlaylist {}.send ();
		};
	}

	menuButton.onClick = [ this ] { showMenu (); };

	playButton.onClick = [ this ]
	{
		if ( ! currentVisible )
			return;

		auto	name = currentVisible->getName ();
		if ( name.isEmpty () )
			return;

		msg::PlayPlaylist { name }.send ();
	};

	UI::setFontRole ( header, UI::fonts::page_title );
	header.setEditable ( true );
	header.setMouseCursor ( juce::MouseCursor::PointingHandCursor );

	header.onTextChange = [ this ]
	{
		// Put the old name back rather than leaving the header blank
		if ( header.getText ().trim ().isEmpty () )
		{
			header.setText ( currentVisible->getName (), juce::dontSendNotification );
			return;
		}

		//
		// Rename currently visible playlist
		//
		const auto	playingRow = currentVisible->getPlayingRow ();
		const auto	selected = currentVisible->getSelectedRows ();

		const auto	oldName = currentVisible->getName ();

		const auto	curPlaylist = pages.getCurrentPlaylist ();
		const auto	curPlaylistName = curPlaylist ? curPlaylist->getName () : juce::String ();

		// Update playlist file; a taken name comes back with a number appendix
		const auto	newName = playlists->renamePlaylist ( oldName, header.getText () );
		currentVisible->setName ( newName );

		removeChildComponent ( currentVisible );
		currentVisible = nullptr;

		// Reload playlists and restore current state
		pages.renamePlaylist ( oldName, newName );

		showPlaylist ( newName );

		// The playing playlist may be the one just renamed, keep tracking
		// it under its new name
		const auto	restoreName = curPlaylistName == oldName ? juce::String ( newName ) : curPlaylistName;
		pages.setCurrentPlaylist ( getPlaylistItems ( restoreName ) );

		// Restore playing row and selected rows
		currentVisible->setPlayingRow ( playingRow );
		currentVisible->setSelectedRows ( selected, juce::dontSendNotification );

		// Load cover artwork, etc.
		pages.updateGrid ();

		pages.playlistGrid.selectPlaylist ( newName );

		// Notify other components of playlist rename
		msg::PlaylistRenamed { oldName, newName }.send ();
	};

	header.onEditorShow = [ this ]
	{
		auto	editor = header.getCurrentTextEditor ();
		editor->setHighlightedRegion ( {} );
		editor->moveCaretToEnd ();
	};

	// Dismissing the editor lands the focus on the list below
	header.onEditorHide = [ this ]
	{
		if ( currentVisible )
			currentVisible->grabKeyboardFocus ();
	};

	coverDisplay.addChangeListener ( this );
	coverDisplay.setInterceptsMouseClicks ( true, true );

	coverDisplay.onPopupMenu = [ this ]
	{
		if ( ! currentVisible )
			return;

		auto	name = currentVisible->getName ();
		if ( name.isEmpty () )
			return;

		auto	m = UI::newPopupMenu ( *this );

		// Delete cover image
		UI::menu_DeleteCover ( m, name );

		UI::showMenuAtMouse ( m, *this );
	};

	addAndMakeVisible ( backButton );
	addAndMakeVisible ( menuButton );
	addAndMakeVisible ( coverDisplay );
	addAndMakeVisible ( playButton );
	addAndMakeVisible ( header );
	addAndMakeVisible ( info );
}
//-----------------------------------------------------------------------------

GUI_Playlist::~GUI_Playlist ()
{
	coverDisplay.removeChangeListener ( this );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::resized ()
{
	juce::Component::resized ();

	auto	listY = std::max ( { backButton.getBottom (), coverDisplay.getBottom (), playButton.getBottom (), header.getBottom (), info.getBottom () } );

	listY += bentoGap;

	listBounds = { bentoGap, listY, getWidth () - bentoGap * 2, getHeight () - listY };

	if ( currentVisible )
		currentVisible->setBounds ( listBounds );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::paint ( juce::Graphics& g )
{
	const auto	b = getLocalBounds ().withHeight ( gradientSize ).toFloat ();
	g.setImageResamplingQuality ( juce::Graphics::mediumResamplingQuality );
	g.drawImage ( blurredBackground, b );

	const auto	scale = g.getInternalContext ().getPhysicalPixelScaleFactor ();

	g.addTransform ( juce::AffineTransform::scale ( 1.0f / scale ) );
	g.setTiledImageFill ( noiseImage, 0.0f, 0.0f, 0.01f );
	g.fillRect ( b * scale );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::lookAndFeelChanged ()
{
	header.setColour ( juce::Label::textColourId, findColour ( UI::colors::text ) );
	changeListenerCallback ( nullptr );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::changeListenerCallback ( juce::ChangeBroadcaster* /*source*/ )
{
	auto	img = coverDisplay.getFinalImage ();
	if ( img.isNull () )
		return;

	img = img.rescaled ( 80, 80, juce::Graphics::highResamplingQuality );
	gin::applyStackBlur ( img, 20 );
	gin::applyHueSaturationLightness ( img, 0.0f, 200.0f, 0.0f );

	blurredBackground = juce::Image ( juce::Image::RGB, 500, gradientSize, true );
	{
		juce::Graphics	g ( blurredBackground );

		g.drawImage ( img, blurredBackground.getBounds ().withHeight ( blurredBackground.getWidth () ).toFloat () );

		const auto	col = UI::getShade ( 0.0f );
		auto	grad = juce::ColourGradient::vertical ( col.withMultipliedAlpha ( 0.1f ), 0.0f, col, gradientSize );

		g.setGradientFill ( grad );
		g.fillRect ( blurredBackground.getBounds () );
		g.fillRect ( blurredBackground.getBounds () );
	}

	// Noise texture
	{
		constexpr auto	noiseSize = 256;

		noiseImage = juce::Image ( juce::Image::RGB, noiseSize, noiseSize, false );
		{
			auto	bitmap = juce::Image::BitmapData ( noiseImage, juce::Image::BitmapData::writeOnly );
			auto	rand = juce::Random ();

			for ( auto y = 0; y < noiseSize; ++y )
			{
				auto	rowPixels = reinterpret_cast<uint8_t*> ( bitmap.getLinePointer ( y ) );

				for ( auto x = 0; x < noiseSize; ++x )
				{
					const auto	luminance = static_cast<uint8_t> ( rand.nextInt ( 256 ) );

					rowPixels[ x * 3	 ] = luminance;
					rowPixels[ x * 3 + 1 ] = luminance;
					rowPixels[ x * 3 + 2 ] = luminance;
				}
			}
		}
	}

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_Playlist::setPlaylists ( const juce::StringArray& list )
{
	if ( currentVisible )
	{
		removeChildComponent ( currentVisible );
		currentVisible = nullptr;
	}

	plMap.clear ();

	for ( const auto& entry : list )
		addPlaylist ( entry.toStdString () );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::updatePlaylist ( const juce::String& name )
{
	// Update playlist items
	auto	items = getPlaylistItems ( name );

	jassert ( items );
	if ( ! items )
		return;

	items->updateRowData ();
	pages.updateGridItem ( name );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::addPlaylist ( const juce::String& filename )
{
	auto	plItem = std::make_unique<GUI_PlaylistItems> ( pages, filename );
	auto	name = plItem->getName ().toStdString ();

	plMap[ name ] = std::move ( plItem );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::renamePlaylist ( const std::string& oldName, const std::string& newName )
{
	// Move item from one key to another in map
	if ( auto it = plMap.find ( oldName ); it != plMap.end () )
	{
		auto	plItem = std::move ( it->second );
		plMap.erase ( it );
		plItem->setName ( newName );
		plMap[ newName ] = std::move ( plItem );
	}
}
//-----------------------------------------------------------------------------

void GUI_Playlist::deletePlaylist ( const juce::String& name )
{
	// Playback may be following this playlist, detach the tracking pointer
	// (and the play queue) before plMap.erase destroys the view
	if ( auto view = getPlaylistItems ( name ); view && pages.getCurrentPlaylist () == view )
		pages.setCurrentPlaylist ( nullptr );

	if ( currentVisible && currentVisible->getName () == name )
	{
		removeChildComponent ( currentVisible );
		currentVisible = nullptr;

		showPlaylist ( "" );
		pages.setPage ( "playlists" );
	}

	plMap.erase ( name.toStdString () );

	// Parked for undo, the files go on commit
	if ( auto parked = playlists->takePlaylist ( name ) )
	{
		const juce::SharedResourcePointer<Strings>		strings;
		const juce::SharedResourcePointer<UndoManager>	undoManager;

		undoManager->arm ( {
			.text = strings->get ( "toast/playlist_deleted" ).replace ( "{}", name ),
			.commit = [ parked ]	{	parked->deleteFile ();	},
			.undo = [ parked ]
			{
				const juce::SharedResourcePointer<Playlists>	playlists;

				playlists->restorePlaylist ( parked );

				// New fills the views, UpdateInfo fills cover and tune info
				msg::PlaylistNew { parked->getName () }.send ();
				msg::PlaylistUpdateInfo { parked->getName () }.send ();
			},
		} );
	}
}
//-----------------------------------------------------------------------------

void GUI_Playlist::showPlaylist ( const juce::String& name )
{
	// Remove current list
	if ( currentVisible && currentVisible->getName () != name )
	{
		removeChildComponent ( currentVisible );
		currentVisible = nullptr;
		playButton.setToggleState ( false, juce::dontSendNotification );
	}

	// Find list in map
	if ( ! currentVisible )
		if ( currentVisible = getPlaylistItems ( name ); ! currentVisible )
			return;

	currentVisible->setBounds ( listBounds );

	header.setText ( name, juce::dontSendNotification );
	updateInfo ();

	addAndMakeVisible ( currentVisible );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::refreshRowData ()
{
	for ( auto& [ name, view ] : plMap )
		view->updateRowData ();

	updateInfo ();
}
//-----------------------------------------------------------------------------

void GUI_Playlist::updateInfo ()
{
	if ( ! currentVisible )
		return;

	const auto	numItems = currentVisible->getSize ();
	auto	playLength = 0u;
	for ( auto i = 0; i < numItems; ++i )
	{
		const auto	item = currentVisible->getItem ( i );
		if ( ! item )	// Missing tunes contribute no play time
			continue;

		auto	subTune = currentVisible->getSubtune ( i );
		if ( subTune == 0 )
			subTune = item->startTune;

		playLength += SID::getTuneLength ( item->file, subTune );
	}

	auto	str = juce::String ( numItems ) + " TUNE";
	if ( numItems > 1 )
		str += "S";
	str += " (";

	str += SID::convertTimeToString ( playLength ) + ")";
	info.setText ( str );

	if ( auto img = currentVisible->getCoverImage (); img.isValid () )
		coverDisplay.setImage ( img );
	else
		coverDisplay.setImages ( pages.getNonEmptyThumbnails ( currentVisible->getName ().toStdString () ) );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::showMenu ()
{
	if ( ! currentVisible )
		return;

	auto	name = currentVisible->getName ();
	if ( name.isEmpty () )
		return;

	auto	m = UI::newPopupMenu ( *this );

	UI::menu_ExportPlaylist ( m, name );
	m.addSeparator ();
	UI::menu_DeleteCover ( m, name );
	m.addSeparator ();
	UI::menu_DeletePlaylist ( m, name );

	UI::showMenuAtButton ( m, *this, menuButton );
}
//-----------------------------------------------------------------------------

GUI_PlaylistItems* GUI_Playlist::getPlaylistItems ( const juce::String& name )
{
	if ( auto it = plMap.find ( name.toStdString () ); it != plMap.end () )
		 return it->second.get ();

	return nullptr;
}
//-----------------------------------------------------------------------------

bool GUI_Playlist::isInterestedInFileDrag ( const juce::StringArray& files )
{
	if ( ! currentVisible )
		return false;

	if ( textutils::getFilteredStrings ( files, { ".png", ".jpg" } ).size () )
		return true;

	return false;
}
//-----------------------------------------------------------------------------

void GUI_Playlist::filesDropped ( const juce::StringArray& files, int /*x*/, int /*y*/ )
{
	if ( ! currentVisible )
		return;

	playlists->applyCoverDrop ( currentVisible->getName (), files );
}
//-----------------------------------------------------------------------------

bool GUI_Playlist::isInterestedInTextDrag ( const juce::String& text )
{
	if ( ! currentVisible )
		return false;

	return textutils::isUrlWithExtension ( text, { ".png", ".jpg" } );
}
//-----------------------------------------------------------------------------

void GUI_Playlist::textDropped ( const juce::String& text, int /*x*/, int /*y*/ )
{
	if ( ! currentVisible )
		return;

	msg::DownloadCover { currentVisible->getName (), text.trim () }.send ();
}
//-----------------------------------------------------------------------------
