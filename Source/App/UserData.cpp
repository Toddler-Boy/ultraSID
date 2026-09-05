#include <algorithm>

#include "UserData.h"

#include "ultra-shared/Helpers/FileUtils.h"

//-----------------------------------------------------------------------------

namespace
{
	// Marks an archive as ours, and carries the writing version
	constexpr auto	manifestName = "ultraSID-user-data.txt";
	constexpr auto	compressionLevel = 6;

	struct Definition
	{
		userdata::Category	category;
		const char*			id;
		juce::StringArray	folders;	// Whole trees under the user root
		juce::StringArray	files;		// Single files under the user root
	};

	const std::vector<Definition>& definitions ()
	{
		static const std::vector<Definition>	defs
		{
			{ userdata::Category::playlists,	"playlists",	{ "Playlists" },							{} },
			{ userdata::Category::likes,		"likes",		{},											{ "likes.txt" } },
			{ userdata::Category::history,		"history",		{},											{ "history.csv" } },
			{ userdata::Category::tunes,		"tunes",		{ "Tunes" },								{ "SID_LUFS.txt" } },
			{ userdata::Category::themes,		"themes",		{ "Themes" },								{} },
			{ userdata::Category::crt,			"crt",			{ "Overlays", "CRT Masks", "CRT Presets" },	{} },
			{ userdata::Category::preferences,	"preferences",	{},											{ "preferences.yml", "chip-profiles.csv" } },
		};

		return defs;
	}

	const Definition& definitionOf ( const userdata::Category category )
	{
		return definitions ()[ size_t ( category ) ];
	}

	// The category an archive entry belongs to; nullopt for the manifest,
	// unknown paths and anything that would escape the root
	std::optional<userdata::Category> categoryOfEntry ( const juce::String& entryName )
	{
		const auto	name = entryName.trimCharactersAtStart ( "/" );

		if ( name.isEmpty () || name.startsWith ( "../" ) || name.contains ( "/../" ) || name.endsWith ( "/.." ) )
			return std::nullopt;

		const auto	hasFolder = name.containsChar ( '/' );
		const auto	first = name.upToFirstOccurrenceOf ( "/", false, false );

		for ( const auto& def : definitions () )
		{
			if ( hasFolder ? def.folders.contains ( first, true ) : def.files.contains ( name, true ) )
				return def.category;
		}

		return std::nullopt;
	}

	// likes.txt: one tune per line, the union stays sorted like Likes writes it
	bool mergeLikes ( const juce::File& file, const juce::String& incoming )
	{
		juce::StringArray	lines;
		file.readLines ( lines );
		lines.addLines ( incoming );
		lines.trim ();
		lines.removeEmptyStrings ();
		lines.removeDuplicates ( false );
		lines.sortNatural ();

		return fileutils::replaceFile ( file, lines.joinIntoString ( "\n" ) );
	}

	// history.csv: header line then one row per play, newest first; the
	// union re-sorts by the trailing ISO date so the reader's order holds
	bool mergeHistory ( const juce::File& file, const juce::String& incoming )
	{
		juce::StringArray	existing;
		file.readLines ( existing );

		auto	header = juce::StringArray::fromLines ( incoming )[ 0 ];
		if ( header.isEmpty () && ! existing.isEmpty () )
			header = existing[ 0 ];

		juce::StringArray	rows;
		rows.addArray ( existing );
		rows.addLines ( incoming );
		rows.trim ();
		rows.removeEmptyStrings ();
		rows.removeString ( header );
		rows.removeDuplicates ( false );

		std::vector<juce::String>	sorted ( rows.begin (), rows.end () );
		std::ranges::stable_sort ( sorted, [] ( const juce::String& a, const juce::String& b )
		{
			return a.fromLastOccurrenceOf ( ",", false, false ) > b.fromLastOccurrenceOf ( ",", false, false );
		} );

		std::string	text = header.toStdString () + '\n';
		for ( const auto& row : sorted )
			text += row.toStdString () + '\n';

		return fileutils::replaceFile ( file, text.data (), text.size () );
	}
}
//-----------------------------------------------------------------------------

juce::String userdata::idOf ( const Category category )
{
	return definitionOf ( category ).id;
}
//-----------------------------------------------------------------------------

juce::Array<juce::File> userdata::listFiles ( const juce::File& root, const Category category )
{
	juce::Array<juce::File>	files;

	if ( ! root.isDirectory () )
		return files;

	const auto&	def = definitionOf ( category );

	for ( const auto& folder : def.folders )
		files.addArray ( root.getChildFile ( folder ).findChildFiles ( juce::File::findFiles | juce::File::ignoreHiddenFiles, true ) );

	for ( const auto& name : def.files )
		if ( const auto f = root.getChildFile ( name ); f.existsAsFile () )
			files.add ( f );

	return files;
}
//-----------------------------------------------------------------------------

int userdata::exportArchive ( const juce::File& root, const std::vector<Category>& categories, const juce::File& zip )
{
	juce::ZipFile::Builder	builder;
	auto	count = 0;

	for ( const auto category : categories )
	{
		for ( const auto& file : listFiles ( root, category ) )
		{
			builder.addFile ( file, compressionLevel, file.getRelativePathFrom ( root ).replaceCharacter ( '\\', '/' ) );
			++count;
		}
	}

	const juce::String	manifest = juce::String ( "ultraSID user data\nversion: " ) + ProjectInfo::versionString + "\n";
	builder.addEntry ( std::make_unique<juce::MemoryInputStream> ( manifest.toRawUTF8 (), manifest.getNumBytesAsUTF8 (), true ),
					   compressionLevel, manifestName, juce::Time::getCurrentTime () );

	juce::TemporaryFile	temp ( zip );

	{
		juce::FileOutputStream	out ( temp.getFile () );

		if ( out.failedToOpen () || ! builder.writeToStream ( out, nullptr ) )
		{
			Z_ERR ( "Could not write " << zip.getFullPathName () );
			return -1;
		}
	}

	if ( ! temp.overwriteTargetFileWithTemporary () )
	{
		Z_ERR ( "Could not write " << zip.getFullPathName () );
		return -1;
	}

	return count;
}
//-----------------------------------------------------------------------------

std::optional<std::vector<userdata::Category>> userdata::inspectArchive ( const juce::File& zipFile )
{
	juce::ZipFile	zip ( zipFile );

	if ( zip.getEntry ( manifestName ) == nullptr )
		return std::nullopt;

	std::vector<Category>	found;

	for ( auto i = 0; i < zip.getNumEntries (); ++i )
	{
		if ( const auto category = categoryOfEntry ( zip.getEntry ( i )->filename ) )
			if ( std::ranges::find ( found, *category ) == found.end () )
				found.push_back ( *category );
	}

	std::ranges::sort ( found );

	return found;
}
//-----------------------------------------------------------------------------

int userdata::importArchive ( const juce::File& root, const juce::File& zipFile, const std::vector<Category>& categories, const Mode mode )
{
	juce::ZipFile	zip ( zipFile );

	if ( zip.getEntry ( manifestName ) == nullptr || ! root.isDirectory () )
		return -1;

	if ( mode == Mode::replace )
	{
		for ( const auto category : categories )
		{
			const auto&	def = definitionOf ( category );

			for ( const auto& folder : def.folders )
				root.getChildFile ( folder ).deleteRecursively ();

			for ( const auto& name : def.files )
				root.getChildFile ( name ).deleteFile ();
		}
	}

	auto	count = 0;

	for ( auto i = 0; i < zip.getNumEntries (); ++i )
	{
		const auto*	entry = zip.getEntry ( i );

		if ( entry->isSymbolicLink || entry->filename.endsWithChar ( '/' ) )
			continue;

		const auto	category = categoryOfEntry ( entry->filename );
		if ( ! category || std::ranges::find ( categories, *category ) == categories.end () )
			continue;

		if ( mode == Mode::merge && ( *category == Category::likes || *category == Category::history ) )
		{
			const std::unique_ptr<juce::InputStream>	in ( zip.createStreamForEntry ( i ) );
			const auto	text = in != nullptr ? in->readEntireStreamAsString () : juce::String ();
			const auto	target = root.getChildFile ( entry->filename.trimCharactersAtStart ( "/" ) );

			if ( in == nullptr || ! ( *category == Category::likes ? mergeLikes ( target, text ) : mergeHistory ( target, text ) ) )
			{
				Z_ERR ( "Could not merge " << entry->filename );
				return -1;
			}
		}
		else if ( const auto result = zip.uncompressEntry ( i, root, true ); result.failed () )
		{
			Z_ERR ( "Could not unpack " << entry->filename << ": " << result.getErrorMessage () );
			return -1;
		}

		++count;
	}

	return count;
}
//-----------------------------------------------------------------------------

bool userdata::copyFolder ( const juce::File& from, const juce::File& to )
{
	if ( ! from.isDirectory () || to.isAChildOf ( from ) || to == from )
		return false;

	if ( to.exists () && to.getNumberOfChildFiles ( juce::File::findFilesAndDirectories ) > 0 )
		return false;

	if ( ! from.copyDirectoryTo ( to ) )
	{
		Z_ERR ( "Could not copy " << from.getFullPathName () << " to " << to.getFullPathName () );
		return false;
	}

	for ( const auto& src : from.findChildFiles ( juce::File::findFiles, true ) )
	{
		const auto	dst = to.getChildFile ( src.getRelativePathFrom ( from ) );

		if ( ! dst.existsAsFile () || dst.getSize () != src.getSize () )
		{
			Z_ERR ( "Copy verification failed for " << dst.getFullPathName () );
			return false;
		}
	}

	return true;
}
//-----------------------------------------------------------------------------
