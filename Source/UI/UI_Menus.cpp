#include <JuceHeader.h>

#include "UI_Menus.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/UndoManager.h"
#include "Config/FilePaths.h"
#include "Data/Playlists.h"
#include "Data/Tags.h"
#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

void UI::menu_AddToPlaylist ( juce::PopupMenu& m, const juce::StringArray& tunes )
{
	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	juce::PopupMenu	submenu;

	// Add to new playlist
	submenu.addItem ( UI::newMenuItem ( strings->get ( "menu/new_playlist" ), icons->get ( "menu/add_to_playlist" ), [ tunes ]
	{
		const juce::SharedResourcePointer<Playlists>	playlists;

		auto	newName = playlists->addToPlaylist ( "", tunes );
		msg::PlaylistNew { newName }.send ();
	} ) );

	submenu.addSeparator ();

	// Add to existing playlist
	{
		const juce::SharedResourcePointer<Playlists>	playlists;

		for ( auto cnt = 0; const auto& name : playlists->getPlaylistNames () )
		{
			submenu.addItem ( UI::newMenuItem ( name, icons->get ( "menu/playlist" ), [ name, tunes ]
			{
				const juce::SharedResourcePointer<Playlists>	playlists;

				playlists->addToPlaylist ( name, tunes );
				msg::PlaylistAddTo { name }.send ();
			} ) );

			if ( ++cnt % 10 == 0 )
				submenu.addColumnBreak ();
		}
	}

	m.addSubMenu ( strings->get ( "menu/add_to_playlist" ), submenu, true, UI::getMenuIcon ( icons->get ( "menu/add_to_playlist" ) ) );
}
//-----------------------------------------------------------------------------

void UI::menu_RemoveFromPlaylist ( juce::PopupMenu& m, const juce::String& plName, const juce::SparseSet<int>& rows )
{
	const juce::SharedResourcePointer<Playlists>	playlists;
	const juce::SharedResourcePointer<Strings>		strings;
	const juce::SharedResourcePointer<Icons>		icons;

	auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );

	// Remove items from playlist
	m.addItem ( UI::newDangerousMenuItem ( strings->get ( "menu/remove_from_playlist" ), icons->get ( "menu/delete" ), [ plName, rows ]
	{
		const juce::SharedResourcePointer<Playlists>	playlists;

		auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );
		if ( ! plItems )
			return;

		// Row/entry pairs for undo re-insertion
		std::vector<std::pair<int, std::string>>	removed;
		removed.reserve ( size_t ( rows.size () ) );

		for ( auto i = 0; i < rows.size (); ++i )
			removed.emplace_back ( rows[ i ], plItems->getEntry ( rows[ i ] ) );

		for ( auto i = rows.size () - 1; i >= 0; --i )
			plItems->removeItem ( rows[ i ], true );

		plItems->saveAndNotify ();

		const juce::SharedResourcePointer<Strings>		strings;
		const juce::SharedResourcePointer<UndoManager>	undoManager;

		const auto	text = rows.size () == 1
						 ? strings->get ( "toast/tune_removed" )
						 : strings->get ( "toast/tunes_removed" ).replace ( "{}", juce::String ( rows.size () ) );

		undoManager->arm ( {
			.text = text,
			.undo = [ plName, removed ]
			{
				const juce::SharedResourcePointer<Playlists>	playlists;

				auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );
				if ( ! plItems )
					return;

				// Ascending order lands every row back at its old index
				for ( const auto& [ row, entry ] : removed )
					plItems->addItem ( entry, row );

				plItems->saveAndNotify ();
			},
		} );

	} ).setEnabled ( plItems && plItems->hasWriteAccess () ) );
}
//-----------------------------------------------------------------------------

void UI::menu_MoveItems ( juce::PopupMenu& m, const juce::String& plName, const juce::SparseSet<int>& rows )
{
	const juce::SharedResourcePointer<Playlists>	playlists;
	const juce::SharedResourcePointer<Strings>		strings;
	const juce::SharedResourcePointer<Icons>		icons;

	auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );

	// Move to top
	m.addItem ( UI::newMenuItem ( strings->get ( "menu/move_to_top" ), icons->get ( "menu/move_to_top" ), [ plName, rows ]
	{
		const juce::SharedResourcePointer<Playlists>	playlists;

		if ( auto plItems = playlists->getPlaylistItems ( plName.toStdString () ) )
		{
			plItems->moveItems ( rows, 0 );
			plItems->saveAndNotify ();
		}
	} ).setEnabled ( plItems && plItems->hasWriteAccess () ) );

	// Move to bottom
	m.addItem ( UI::newMenuItem ( strings->get ( "menu/move_to_bottom" ), icons->get ( "menu/move_to_bottom" ), [ plName, rows ]
	{
		const juce::SharedResourcePointer<Playlists>	playlists;

		if ( auto plItems = playlists->getPlaylistItems ( plName.toStdString () ) )
		{
			plItems->moveItems ( rows, plItems->getNumItems () );
			plItems->saveAndNotify ();
		}
	} ).setEnabled ( plItems && plItems->hasWriteAccess () ) );
}
//-----------------------------------------------------------------------------

void UI::menu_GoToFolder ( juce::PopupMenu& m, const juce::String& folder )
{
	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	// TODO: re-implment this differently
	auto	authorStr = strings->get ( "menu/go_to_folder" );

	if ( folder.isEmpty () )
		authorStr += " <multiple>";
	else
		authorStr += " " + folder;

	m.addItem ( UI::newMenuItem ( authorStr, icons->get ( "menu/go_to_folder" ), [ folder ]
	{
		msg::GoToFolder { folder }.send ();

	} ).setEnabled ( folder.isNotEmpty () ) );
}
//-----------------------------------------------------------------------------

void UI::menu_ExportTrack ( juce::PopupMenu& m, const juce::StringArray& tunes )
{
	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	m.addItem ( UI::newMenuItem ( strings->get ( tunes.size () > 1 ? "menu/export_tunes" : "menu/export_tune" ), icons->get ( "menu/export_tune" ), [ tunes ]
	{
		msg::ExportTune { tunes }.send ();
	} ) );
}
//-----------------------------------------------------------------------------

void UI::menu_ExportPlaylist ( juce::PopupMenu& m, const juce::String& plName )
{
	const juce::SharedResourcePointer<Playlists>	playlists;
	const juce::SharedResourcePointer<Strings>		strings;
	const juce::SharedResourcePointer<Icons>		icons;

	auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );

	// Queue every playlist entry for export
	m.addItem ( UI::newMenuItem ( strings->get ( "menu/export_playlist" ), icons->get ( "menu/export_tune" ), [ plName ]
	{
		const juce::SharedResourcePointer<Playlists>	playlists;

		auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );
		if ( ! plItems )
			return;

		juce::StringArray	tunes;

		for ( auto i = 0; i < plItems->getNumItems (); ++i )
			tunes.add ( plItems->getEntry ( i ) );

		msg::ExportTune { tunes }.send ();

	} ).setEnabled ( plItems && plItems->getNumItems () > 0 ) );
}
//-----------------------------------------------------------------------------

void UI::menu_DeleteCover ( juce::PopupMenu& m, const juce::String& plName )
{
	const juce::SharedResourcePointer<Playlists>	playlists;
	const juce::SharedResourcePointer<Strings>		strings;
	const juce::SharedResourcePointer<Icons>		icons;

	auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );

	m.addItem ( UI::newDangerousMenuItem ( strings->get ( "menu/delete_cover" ), icons->get ( "menu/delete" ), [ plName ]
	{
		const juce::SharedResourcePointer<Playlists>	playlists;

		auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );
		if ( ! plItems || ! plItems->hasCover () )
			return;

		// Only the in-memory cover goes now, the file goes on commit
		const auto	coverFile = plItems->getCoverFile ();
		const auto	cover = plItems->takeCover ();

		msg::PlaylistUpdateInfo { plName }.send ();

		const juce::SharedResourcePointer<Strings>		strings;
		const juce::SharedResourcePointer<UndoManager>	undoManager;

		undoManager->arm ( {
			.text = strings->get ( "toast/cover_deleted" ),
			.commit = [ coverFile ]	{	coverFile.deleteFile ();	},
			.undo = [ plName, cover ]
			{
				const juce::SharedResourcePointer<Playlists>	playlists;

				if ( auto plItems = playlists->getPlaylistItems ( plName.toStdString () ) )
					plItems->restoreCover ( cover );

				msg::PlaylistUpdateInfo { plName }.send ();
			},
		} );

	} ).setEnabled ( plItems && plItems->hasCover () ) );
}
//-----------------------------------------------------------------------------

void UI::menu_DeletePlaylist ( juce::PopupMenu& m, const juce::String& plName )
{
	const juce::SharedResourcePointer<Playlists>	playlists;
	const juce::SharedResourcePointer<Strings>		strings;
	const juce::SharedResourcePointer<Icons>		icons;

	auto	plItems = playlists->getPlaylistItems ( plName.toStdString () );

	m.addItem ( UI::newDangerousMenuItem ( strings->get ( "menu/delete_playlist" ), icons->get ( "menu/delete" ), [ plName ]
	{
		msg::PlaylistDeleteList { plName }.send ();

	} ).setEnabled ( plItems ) );
}
//-----------------------------------------------------------------------------

void UI::menu_ClearOlderThan ( juce::PopupMenu& m, std::function<void ( double days )> clear )
{
	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	static constexpr struct { const char* name; double days; } periods[] =
	{
		{ "menu/older_1_month",  30.0  },
		{ "menu/older_3_months", 90.0  },
		{ "menu/older_6_months", 180.0 },
		{ "menu/older_1_year",   365.0 },
	};

	juce::PopupMenu	clearOlder;

	for ( const auto& p : periods )
		clearOlder.addItem ( UI::newMenuItem ( strings->get ( p.name ), icons->get ( "menu/clear_older" ), [ clear, days = p.days ]
		{
			clear ( days );
		} ) );

	m.addSubMenu ( strings->get ( "menu/clear_older" ), clearOlder, true, UI::getMenuIcon ( icons->get ( "menu/clear_older" ) ) );
}
//-----------------------------------------------------------------------------

void UI::menu_ToggleTag ( juce::PopupMenu& m, const juce::StringArray& tunes )
{
	const juce::SharedResourcePointer<Tags>		tags;
	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	for ( const auto& tag : tags->getTagEntries () )
	{
		m.addItem ( UI::newMenuItem ( strings->get ( "menu/toggle_tag" ) + " " + strings->get ( tag.name ), icons->get ( tag.name ), [ name = tag.name, tunes, tags ]
		{
			tags->toggleTags ( name, tunes );
			msg::TagsToggled {}.send ();
		} ) );
	}

	// Copies the tune keys sans location marker, the exact strings the
	// audio/chip-profile CSVs key on
	if ( buildinfo::isDeveloperMode () && ! tunes.isEmpty () )
	{
		m.addSeparator ();
		m.addItem ( "Copy CSV path", [ tunes ]
		{
			juce::StringArray	paths;

			for ( const auto& tune : tunes )
				paths.add ( filepaths::stripLocationMarker ( tune.toStdString () ) );

			juce::SystemClipboard::copyTextToClipboard ( paths.joinIntoString ( "\n" ) );
		} );
	}
}
//-----------------------------------------------------------------------------
