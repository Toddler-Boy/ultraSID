#include "HVSCUpdater.h"

//-----------------------------------------------------------------------------

int HVSCUpdater::update ( HVSCTree& _tree, const int updateVersion, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles )
{
	progress = 0.0f;
	files = 0;
	maxFiles = 0;

	tree = &_tree;

	//
	// Execute commands from the "UpdateXX.hvs"-file to update the HVSC
	//
	const auto	updateName = juce::String ( "Update" + juce::String ( updateVersion ) + ".hvs" );

	if ( ! loadLines ( tree->read ( "update/" + updateName ) ) )
		return 1;

	maxFiles = int ( lines.size () );

	//
	// Index the entire collection for case-correcting script paths
	//
	for ( const auto& pathStr : tree->listAllForCaseIndex () )
		caseFiles.insert ( pathStr.toStdString () );

	// Loop over strings
	auto	skipLinesAfterError = 0;
	while ( ! endOfLines () )
	{
		const auto	nLine = getNextLine ();

		files.store ( lineIndex );
		progress = float ( std::min ( lineIndex, maxFiles.load () ) / float ( maxFiles ) );
		const auto	x = std::ranges::find ( keywords, nLine );
		if ( x != keywords.end () )
		{
			mode = mode_type ( x - keywords.begin () );
			keyword = nLine;
			Z_DLOG ( "New mode: " << keyword );
			continue;
		}

		if ( keyword.isEmpty () )
		{
			logError ( juce::String ( "Unknown keyword (" ) + nLine + ")" );
			continue;
		}

		if ( mode == COPYRIGHT )
			mode = RELEASED;

		switch ( mode )
		{
			case     TITLE:
			case    AUTHOR:
			case  RELEASED:
			case   CREDITS:
			case     SPEED:
			case     SONGS:
			case   FIXLOAD:
			case  INITPLAY:
			case MUSPLAYER:
			case   PLAYSID:
			case     CLOCK:
			case  SIDMODEL:
			case FREEPAGES:
			case     FLAGS:
				{
					const auto	sidName = juce::String ( toFilename ( nLine ) );
					if ( ! fileExistsAsFile ( sidName, true ) )
					{
						if		( mode == CREDITS )		skipLinesAfterError = 3;
						else if ( mode == FLAGS )		skipLinesAfterError = 4;
						else if ( mode == FIXLOAD )		skipLinesAfterError = 0;
						else							skipLinesAfterError = 1;
					}
					else
					{
						char	sidInfo[ 4 ][ maxSidInfoLen + 1 ] = {};

						if ( mode == CREDITS )
						{
							for ( auto n = 0; n < 3; n++ )
							{
								auto	line = getNextLine ();

								if ( line.empty () )
									logError ( "Premature end of update script?" );

								if ( line.size () > maxSidInfoLen )
									logError ( "SID credit string too long" );

								std::strncpy ( sidInfo[ n ], line.c_str (), maxSidInfoLen );
							}
						}
						else if ( mode == FLAGS )
						{
							for ( auto n = 0; n < 4; n++ )
							{
								auto	line = getNextLine ();

								if ( line.empty () )
									logError ( "Premature end of update script?" );

								std::strncpy ( sidInfo[ n ], line.c_str (), maxSidInfoLen );
							}
						}
						else if ( mode == AUTHOR || mode == TITLE || mode == RELEASED )
						{
							auto	line = getNextLine ();
							if ( line.empty () )
								logError ( "Premature end of update script?" );

							if ( line.size () > maxSidInfoLen )
								logError ( "SID credit string too long" );

							std::strncpy ( sidInfo[ mode ], line.c_str (), maxSidInfoLen );
						}
						else if ( mode == FIXLOAD )
						{
							;// No additional parameters
						}
						else
						{
							// SPEED, SONGS, INITPLAY, MUSPLAYER, PLAYSID, CLOCK,
							// SIDMODEL, FREEPAGES
							auto	line = getNextLine ();

							if ( line.empty () )
								logError ( "Premature end of update script?" );

							std::strncpy ( sidInfo[ 0 ], line.c_str (), maxSidInfoLen );
						}

						//
						// Update SID file itself
						//
						{
							updater_sidTune	sidFileInfo ( tree->read ( sidName ) );

							if ( ! sidFileInfo.getStatus () )
								logError ( "Could not load file /" + sidName );
							else
							{
								if ( ! sidFileInfo.writeToSidTune ( sidInfo, mode ) )
									logError ( "Problem writing new sidtune info for file /" + sidName );
								else if ( const auto out = sidFileInfo.savePSIDData (); out.getSize () == 0 || ! tree->write ( sidName, out ) )
									logError ( "Could not save file /" + sidName );
							}
						}
					}
				}
				break;

			case DELETEMODE:
				{
					const auto	fileName = juce::String ( toFilename ( nLine ) );

					if ( fileExists ( fileName, true ) )
						deleteFile ( fileName, true );
				}
				break;

			case	MOVE:
			case REPLACE:
				{
					const auto	srcFileName = toFilename ( nLine );
					const auto	srcFile = juce::String ( srcFileName );

					fileExists ( srcFile );

					const auto	dstFileName = toFilename ( getNextLine () );

					if ( dstFileName.empty () )
						logError ( "Premature end of update script" );

					if ( srcFileName.empty () )
						break;

					const auto	srcIsDir = srcFileName.ends_with ( '/' );
					const auto	dstIsDir = dstFileName.ends_with ( '/' );

					const auto	dstFile = juce::String ( dstFileName );
					if ( ! fileExists ( dstFile ) )
						if ( dstIsDir && ! createDirectory ( dstFile ) )
							break;

					// Now a destination dir or file should exist
					if ( srcIsDir )
					{
						if ( ! dstIsDir )
						{
							logError ( "Destination is not a directory. " + dstFile );
							break;
						}

						// Merge a directory into another directory
						for ( const auto& name : tree->listChildFiles ( srcFile ) )
						{
							const auto	newDstFile = dstFile + name;

							if ( mode == REPLACE )
								deleteFile ( newDstFile );

							moveFileTo ( srcFile + name, newDstFile );
						}
					}
					else
					{
						// Move single file to destination dir
						const auto	newDstFile = dstIsDir ? dstFile + srcFile.fromLastOccurrenceOf ( "/", false, false ) : dstFile;

						if ( mode == REPLACE )
							deleteFile ( newDstFile );

						moveFileTo ( srcFile, newDstFile );
					}
				}
				break;

			case MKDIRMODE:
				{
					Z_WARN ( "HVSC update: MKDIRMODE command is obsolete in line : " << lineIndex );
				}
				break;

			case NO_MODE:
			default:
				logError ( "Keyword/parameter mismatch?" );
				break;
		}

		while ( skipLinesAfterError )
		{
			getNextLine ();
			--skipLinesAfterError;
		}
	}

	// Deliberately the LAST operations, only on a clean run: the .hvs in
	// DOCUMENTS is the "applied" marker (checked by HVSCInstaller). A failed
	// run leaves none, so a retry starts clean
	if ( errorCount == 0 )
	{
		const auto	dstUpdateFile = juce::String ( toFilename ( "/DOCUMENTS/" + updateName.toStdString () ) );
		moveFileTo ( "update/" + updateName, dstUpdateFile );

		tree->removeTree ( "update" );
	}
	else
	{
		Z_ERR ( "HVSC " << updateName << " failed" );
	}

	return errorCount;
}
//-----------------------------------------------------------------------------

bool HVSCUpdater::loadLines ( const juce::MemoryBlock& data )
{
	keyword = {};
	errorCount = 0;

	lines.clear ();
	lineIndex = 0;

	// Raw bytes: the credit strings are extended ASCII, not UTF-8
	if ( data.isEmpty () )
		return false;

	const auto*	text = static_cast<const char*> ( data.getData () );

	std::string	line;
	for ( std::size_t i = 0; i < data.getSize (); ++i )
	{
		if ( const auto c = text[ i ]; c == '\n' )
		{
			lines.emplace_back ( std::move ( line ) );
			line.clear ();
		}
		else if ( c != '\r' )
			line += c;
	}

	if ( ! line.empty () )
		lines.emplace_back ( std::move ( line ) );

	// Remove empty lines at end
	auto	it = std::find_if_not ( lines.rbegin (), lines.rend (),
								 [] ( const std::string& s ) { return s.empty (); } );

	lines.erase ( it.base (), lines.end () );

	lines.shrink_to_fit ();

	return lines.size () > 1;
}
//-----------------------------------------------------------------------------

const std::string& HVSCUpdater::getNextLine ()
{
	// Skip comments
	while ( lineIndex < int ( lines.size () ) && ( lines[ lineIndex ].empty () || lines[ lineIndex ].starts_with ( ';' ) || lines[ lineIndex ].starts_with ( '#' ) ) )
		++lineIndex;

	if ( endOfLines () )
	{
		static const	std::string	emptyString;
		return emptyString;
	}

	return lines[ lineIndex++ ];
}
//-----------------------------------------------------------------------------

std::string HVSCUpdater::toFilename ( std::string in )
{
	if ( in.empty () )
		return in;

	// Normalize separators
	std::replace ( in.begin (), in.end (), '\\', '/' );

	// Fix typos in update script
	{
		// Check for leading slash
		if ( ! in.starts_with ( '/' ) )
			in.insert ( 0, 1, '/' );

		// Check for trailing slash for directories
		if ( ! in.ends_with ( '/' ) )
			if ( const auto lastSlash = in.find_last_of ( '/' ); lastSlash != std::string::npos )
				if ( ! in.substr ( lastSlash ).contains ( '.' ) )
					in += '/';
	}

	// Case correct the input
	if ( auto it = caseFiles.find ( in ); it != caseFiles.end () )
		return it->substr ( 1 );

	// Path not found, remove last segment of the path and try again (allows to create new paths with corrected parent)
	const auto	isDir = in.ends_with ( '/' );
	if ( isDir )
		in = in.substr ( 0, in.length () - 1 );

	// Returns filename with corrected parent-path, and passed in filename
	const auto	lastSlash = in.find_last_of ( '/' );
	const auto	parentPath = in.substr ( 0, lastSlash + 1 );

	if ( auto it = caseFiles.find ( parentPath ); it != caseFiles.end () )
	{
		if ( isDir )
			return it->substr ( 1 ) + in.substr ( lastSlash + 1 ) + '/';
		else
			return it->substr ( 1 ) + in.substr ( lastSlash + 1 );
	}

	// Couldn't find parent either, this should not happen
	Z_WARN ( "HVSC update: Couldn't correct path for " << in << " in line " << lineIndex );

	return in.substr ( 1 );
}
//-----------------------------------------------------------------------------

void HVSCUpdater::logError ( const juce::String& message )
{
	Z_ERR ( "HVSC update: " << message << " in line " << lineIndex);
	++errorCount;
}
//-----------------------------------------------------------------------------

bool HVSCUpdater::fileExists ( const juce::String& rel, const bool withError )
{
	const auto	ret = tree->exists ( rel );

	if ( ! ret && withError )
		logError ( "No such path or permission denied for /" + rel );

	return ret;
}
//-----------------------------------------------------------------------------

bool HVSCUpdater::fileExistsAsFile ( const juce::String& rel, const bool withError )
{
	const auto	ret = tree->existsAsFile ( rel );

	if ( ! ret && withError )
		logError ( "No such file or permission denied for /" + rel );

	return ret;
}
//-----------------------------------------------------------------------------

bool HVSCUpdater::deleteFile ( const juce::String& rel, const bool withError )
{
	const auto	ret = tree->remove ( rel );

	if ( ! ret && withError )
		logError ( "Could not delete file or directory /" + rel );

	caseFiles.erase ( ( "/" + rel ).toStdString () );

	return ret;
}
//-----------------------------------------------------------------------------

bool HVSCUpdater::createDirectory ( const juce::String& rel )
{
	const auto	ret = tree->mkdir ( rel );

	if ( ! ret )
		logError ( "Creation of directory failed /" + rel );
	else
		caseFiles.insert ( ( "/" + rel + ( rel.endsWithChar ( '/' ) ? "" : "/" ) ).toStdString () );

	return ret;
}
//-----------------------------------------------------------------------------

bool HVSCUpdater::moveFileTo ( const juce::String& from, const juce::String& to )
{
	const auto	ret = tree->move ( from, to );

	if ( ! ret )
		logError ( "Could not move source file /" + from + " => /" + to );
	else
	{
		caseFiles.erase ( ( "/" + from ).toStdString () );
		caseFiles.insert ( ( "/" + to ).toStdString () );
	}
	return ret;
}
//-----------------------------------------------------------------------------
