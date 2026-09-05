#include <unordered_set>

#include "HVSCTree.h"

#include "ultra-shared/Config/ZipFolder.h"
#include "ultra-shared/Helpers/FileUtils.h"

//-----------------------------------------------------------------------------

namespace
{
	// Script paths write directories with a trailing '/'; file APIs want none
	[[ nodiscard ]] juce::String trimmed ( const juce::String& rel )
	{
		return rel.endsWithChar ( '/' ) ? rel.dropLastCharacters ( 1 ) : rel;
	}
}
//-----------------------------------------------------------------------------

juce::File FolderTree::fileFor ( const juce::String& rel ) const
{
	return root.getChildFile ( trimmed ( rel ) );
}
//-----------------------------------------------------------------------------

bool FolderTree::exists ( const juce::String& rel ) const
{
	return fileFor ( rel ).exists ();
}
//-----------------------------------------------------------------------------

bool FolderTree::existsAsFile ( const juce::String& rel ) const
{
	return fileFor ( rel ).existsAsFile ();
}
//-----------------------------------------------------------------------------

juce::StringArray FolderTree::listAllForCaseIndex () const
{
	const auto	rootLen = root.getFullPathName ().length ();

	juce::StringArray	ret;

	for ( const auto& entry : juce::RangedDirectoryIterator ( root, true, "*", juce::File::findFilesAndDirectories ) )
	{
		auto	pathStr = entry.getFile ().getFullPathName ().substring ( rootLen ).replaceCharacter ( '\\', '/' );

		if ( entry.isDirectory () )
			pathStr += '/';

		ret.add ( pathStr );
	}

	return ret;
}
//-----------------------------------------------------------------------------

juce::StringArray FolderTree::listChildFiles ( const juce::String& folder ) const
{
	juce::StringArray	ret;

	for ( const auto& f : fileFor ( folder ).findChildFiles ( juce::File::findFiles, false, "*", juce::File::FollowSymlinks::no ) )
		ret.add ( f.getFileName () );

	return ret;
}
//-----------------------------------------------------------------------------

juce::MemoryBlock FolderTree::read ( const juce::String& rel ) const
{
	juce::MemoryBlock	mb;
	fileFor ( rel ).loadFileAsData ( mb );

	return mb;
}
//-----------------------------------------------------------------------------

bool FolderTree::write ( const juce::String& rel, const juce::MemoryBlock& data )
{
	return fileutils::replaceFile ( fileFor ( rel ), data.getData (), data.getSize () );
}
//-----------------------------------------------------------------------------

bool FolderTree::remove ( const juce::String& rel )
{
	return fileFor ( rel ).deleteFile ();
}
//-----------------------------------------------------------------------------

bool FolderTree::removeTree ( const juce::String& rel )
{
	return fileFor ( rel ).deleteRecursively ();
}
//-----------------------------------------------------------------------------

bool FolderTree::mkdir ( const juce::String& rel )
{
	return fileFor ( rel ).createDirectory ().wasOk ();
}
//-----------------------------------------------------------------------------

bool FolderTree::move ( const juce::String& from, const juce::String& to )
{
	return fileFor ( from ).moveFileTo ( fileFor ( to ) );
}
//-----------------------------------------------------------------------------

std::string ZipTree::ghostKey ( const juce::String& folder )
{
	return ( trimmed ( folder ).toLowerCase () + "/" ).toStdString ();
}
//-----------------------------------------------------------------------------

bool ZipTree::exists ( const juce::String& rel ) const
{
	return zip.exists ( prefix + trimmed ( rel ) ) || zip.folderExists ( prefix + trimmed ( rel ) ) || ghostDirs.contains ( ghostKey ( rel ) );
}
//-----------------------------------------------------------------------------

bool ZipTree::existsAsFile ( const juce::String& rel ) const
{
	return zip.exists ( prefix + trimmed ( rel ) );
}
//-----------------------------------------------------------------------------

juce::StringArray ZipTree::listAllForCaseIndex () const
{
	juce::StringArray				ret;
	std::unordered_set<std::string>	dirs;

	// Directories exist through their files, so every parent chain counts once
	for ( const auto& f : zip.listFiles ( prefix, true ) )
	{
		ret.add ( "/" + f );

		for ( auto pos = f.indexOfChar ( '/' ); pos >= 0; pos = f.indexOfChar ( pos + 1, '/' ) )
			dirs.insert ( ( "/" + f.substring ( 0, pos + 1 ) ).toStdString () );
	}

	for ( const auto& d : dirs )
		ret.add ( juce::String ( d ) );

	return ret;
}
//-----------------------------------------------------------------------------

juce::StringArray ZipTree::listChildFiles ( const juce::String& folder ) const
{
	return zip.listFiles ( prefix + trimmed ( folder ), false );
}
//-----------------------------------------------------------------------------

juce::MemoryBlock ZipTree::read ( const juce::String& rel ) const
{
	return zip.load ( prefix + rel );
}
//-----------------------------------------------------------------------------

bool ZipTree::write ( const juce::String& rel, const juce::MemoryBlock& data )
{
	zip.writeFile ( prefix + rel, data );

	return true;
}
//-----------------------------------------------------------------------------

bool ZipTree::remove ( const juce::String& rel )
{
	if ( zip.exists ( prefix + trimmed ( rel ) ) )
	{
		ghostDirs.erase ( ghostKey ( rel ) );
		return zip.remove ( prefix + trimmed ( rel ) );
	}

	// A folder still holding files refuses deletion; an emptied one goes by
	// dropping its ghost
	if ( zip.folderExists ( prefix + trimmed ( rel ) ) )
		return false;

	return ghostDirs.erase ( ghostKey ( rel ) ) > 0;
}
//-----------------------------------------------------------------------------

bool ZipTree::removeTree ( const juce::String& rel )
{
	ghostDirs.erase ( ghostKey ( rel ) );

	return zip.removeFolder ( prefix + trimmed ( rel ) );
}
//-----------------------------------------------------------------------------

bool ZipTree::mkdir ( const juce::String& rel )
{
	ghostDirs.insert ( ghostKey ( rel ) );

	return true;
}
//-----------------------------------------------------------------------------

bool ZipTree::move ( const juce::String& from, const juce::String& to )
{
	if ( ! zip.rename ( prefix + trimmed ( from ), prefix + trimmed ( to ) ) )
		return false;

	// The emptied source chain stays deletable, like the skeleton dirs a
	// loose-tree move leaves behind
	for ( auto parent = from.upToLastOccurrenceOf ( "/", true, false ); parent.isNotEmpty (); parent = parent.dropLastCharacters ( 1 ).upToLastOccurrenceOf ( "/", true, false ) )
		ghostDirs.insert ( ghostKey ( parent ) );

	return true;
}
//-----------------------------------------------------------------------------

bool ZipTree::finish ()
{
	return zip.commit ();
}
//-----------------------------------------------------------------------------
