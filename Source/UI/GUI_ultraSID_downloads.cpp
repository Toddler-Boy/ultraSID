#include "Helpers/Messages.h"

#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

void GUI_ultraSID::startFullInstall ()
{
	// A zip is replaced whole, a folder is emptied first; only an existing
	// install warrants the warning
	const auto	occupied = roots.hvsc.hasFileExtension ( "zip" )
						 ? roots.hvsc.existsAsFile ()
						 : roots.hvsc.isDirectory () && roots.hvsc.getNumberOfChildFiles ( juce::File::findFilesAndDirectories ) > 0;

	if ( ! occupied )
	{
		hvscInstaller.downloadFull ();
		return;
	}

	juce::NativeMessageBox::showYesNoBox ( juce::MessageBoxIconType::WarningIcon,
		strings->get ( "install/replace_title" ),
		strings->get ( "install/replace_message" ).replace ( "{}", roots.hvsc.getFullPathName () ),
		this,
		juce::ModalCallbackFunction::create ( [ safe = juce::Component::SafePointer<GUI_ultraSID> ( this ) ] ( int r )
		{
			if ( r == 1 && safe != nullptr )
				safe->hvscInstaller.downloadFull ();
		} )
	);
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::downloadScreenshot ( const juce::String& dlUrl )
{
	downloader.startAsyncDownload ( dlUrl, [ this ] ( gin::DownloadManager::DownloadResult res )
	{
		if ( res.ok )
		{
			auto	filename = res.url.getSubPath ().fromLastOccurrenceOf ( "/", false, false );

			if ( filename.endsWithIgnoreCase ( ".png" ) )
			{
				auto	tempFile = juce::File::getSpecialLocation ( juce::File::tempDirectory ).getChildFile ( filename );

				gin::overwriteWithData ( tempFile, res.data );

				addScreenshots ( { tempFile.getFullPathName () } );
			}
		}
		else
		{
			Z_ERR ( "Download failed HTTP/" << res.httpCode << " for " << res.url.toString ( true ) );
		}
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::downloadCoverImage ( const juce::String& targetPlaylist, juce::StringArray dlUrls )
{
	if ( dlUrls.isEmpty () )
		return;

	const auto	dlUrl = dlUrls[ 0 ];
	dlUrls.remove ( 0 );

	downloader.startAsyncDownload ( dlUrl, [ this, targetPlaylist, dlUrls ] ( gin::DownloadManager::DownloadResult res )
	{
		if ( res.ok )
		{
			auto	filename = res.url.getSubPath ().fromLastOccurrenceOf ( "/", false, false );

			if ( filename.endsWithIgnoreCase ( ".png" ) || filename.endsWithIgnoreCase ( ".jpg" ) )
			{
				auto	tempFile = juce::File::getSpecialLocation ( juce::File::tempDirectory ).getChildFile ( filename );

				gin::overwriteWithData ( tempFile, res.data );

				playlists->setPlaylistCover ( targetPlaylist, tempFile.getFullPathName () );
				msg::PlaylistUpdateInfo { targetPlaylist }.send ();

				tempFile.deleteFile ();
			}
		}
		else if ( dlUrls.isEmpty () )
		{
			Z_ERR ( "Download failed HTTP/" << res.httpCode << " for " << res.url.toString ( true ) );
		}
		else
		{
			downloadCoverImage ( targetPlaylist, dlUrls );
		}
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::downloadPlaylist ( const juce::URL& dlUrl )
{
	downloader.startAsyncDownload ( dlUrl, [ this ] ( gin::DownloadManager::DownloadResult res )
	{
		if ( ! res.ok )
		{
			Z_ERR ( "Download failed HTTP/" << res.httpCode << " for " << res.url.toString ( true ) );
			return;
		}

		const auto	name = juce::URL::removeEscapeChars ( res.url.getFileName () ).upToLastOccurrenceOf ( ".", false, false );
		const auto	imported = playlists->importPlaylist ( name, res.data.toString () );

		if ( imported.name.isEmpty () )
		{
			Z_ERR ( "Not an M3U playlist: " << res.url.toString ( true ) );
			return;
		}

		if ( imported.created )
			msg::PlaylistNew { imported.name }.send ();

		msg::ShowPlaylist { imported.name }.send ();

		const auto	base = res.url.getSubPath ().upToLastOccurrenceOf ( ".", false, false );

		downloadCoverImage ( imported.name, { res.url.withNewSubPath ( base + ".jpg" ).toString ( false ),
											  res.url.withNewSubPath ( base + ".png" ).toString ( false ) } );
	} );
}
//-----------------------------------------------------------------------------

// The HVSC install/update flow and image downloads
void GUI_ultraSID::registerDownloadActions ()
{
	router.on<msg::DownloadHVSC> ( [ this ] ( const auto& e )
	{
		switch ( e.what )
		{
			using enum msg::DownloadHVSC::action;
			case update:		hvscInstaller.downloadUpdate ();	break;
			case full:			startFullInstall ();				break;
			case cancel:		hvscInstaller.cancelFullInstall ();	break;
			case cancelUpdate:	hvscInstaller.cancelUpdate ();		break;
		}
	} );

	// The installed collection against the release this build targets
	router.on<msg::HvscCheck> ( [ this ]
	{
		if ( updateHVSCScreen.isUpdating () )
		{
			if ( installState->hvsc.needsUpdate () )
			{
				hvscInstaller.downloadUpdate ();
				return;
			}
			showPage ( "updateHVSC" );
			return;
		}

		// Too old to patch: onboarding offers the full reinstall
		if ( installState->hvsc.needsFullInstall () )
		{
			onboardingScreen.startOver ();
			showPage ( "onboarding" );
			return;
		}

		// Installed but behind the release this build ships for
		if ( installState->hvsc.needsUpdate () )
		{
			showPage ( "updateHVSC" );
			return;
		}

		// Installed and current
		showPage ( "search" );
	} );

	router.on<msg::DownloadScreenshot> ( [ this ] ( const auto& e )	{	downloadScreenshot ( e.url.trim () );	} );
	router.on<msg::DownloadCover> ( [ this ] ( const auto& e )		{	downloadCoverImage ( e.playlist, { e.url } );	} );
}
//-----------------------------------------------------------------------------

