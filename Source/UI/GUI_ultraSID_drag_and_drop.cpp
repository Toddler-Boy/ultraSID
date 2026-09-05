#include "ultra-shared/Helpers/TextUtils.h"

#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

bool GUI_ultraSID::isInterestedInFileDrag ( const juce::StringArray& files )
{
	for ( const auto& f : files )
		if ( f.endsWithIgnoreCase ( ".sid" ) || f.endsWithIgnoreCase ( ".m3u" ) )
			 return true;

	return false;
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::filesDropped ( const juce::StringArray& files, int /*x*/, int /*y*/ )
{
	juce::StringArray	sidFiles;
	juce::StringArray	playlistFiles;

	for ( const auto& f : files )
	{
		if ( f.endsWithIgnoreCase ( ".sid" ) )
			sidFiles.add ( f );
		else if ( f.endsWithIgnoreCase ( ".m3u" ) )
			playlistFiles.add ( f );
	}

	addSidTunes ( sidFiles );
	addPlaylistFiles ( playlistFiles );
}
//-----------------------------------------------------------------------------

bool GUI_ultraSID::isInterestedInTextDrag ( const juce::String& text )
{
	// Any csdb.dk link is welcome regardless of its file name
	const auto	trimmed = text.trim ().toLowerCase ();

	if ( trimmed.startsWith ( "http://" ) || trimmed.startsWith ( "https://" ) )
		if ( juce::URL ( trimmed ).getDomain ().equalsIgnoreCase ( "csdb.dk" ) )
			return true;

	return textutils::isUrlWithExtension ( text, { ".sid", ".m3u" } );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::textDropped ( const juce::String& text, int /*x*/, int /*y*/ )
{
	const auto	dlURL = juce::URL ( text.trim () );

	if ( dlURL.getSubPath ().endsWithIgnoreCase ( ".m3u" ) )
	{
		downloadPlaylist ( dlURL );
		return;
	}

	if ( ! dlURL.getDomain ().equalsIgnoreCase ( "csdb.dk" ) || ! dlURL.getSubPath ().endsWithIgnoreCase ( ".sid" ) )
		return;

	downloader.startAsyncDownload ( dlURL, [ this ] ( gin::DownloadManager::DownloadResult res )
	{
		if ( res.ok )
		{
			auto	filename = res.url.getSubPath ().fromLastOccurrenceOf ( "/", false, false );

			if ( res.url.getDomain ().equalsIgnoreCase ( "csdb.dk" ) )
				filename = "";

			if ( filename.isEmpty () )
			{
				// Parse content-disposition to get filename
				auto	dispo = juce::StringArray::fromTokens ( res.responseHeaders.getValue ( "content-disposition", "" ), ";", "" );
				dispo.trim ();
				dispo.removeEmptyStrings ();

				for ( const auto& d : dispo )
				{
					if ( d.startsWithIgnoreCase ( "filename=" ) )
					{
						filename = d.fromFirstOccurrenceOf ( "=", false, false ).unquoted ().trim ();
						break;
					}
				}
			}

			if ( filename.isNotEmpty () )
			{
				auto	tempFile = juce::File::getSpecialLocation ( juce::File::tempDirectory ).getChildFile ( filename );

				gin::overwriteWithData ( tempFile, res.data );

				addSidTunes ( { tempFile.getFullPathName () } );
			}
		}
		else
		{
			Z_ERR ( "Download failed HTTP/" << res.httpCode << " for " << res.url.toString ( true ) );
		}
	} );
}
//-----------------------------------------------------------------------------
