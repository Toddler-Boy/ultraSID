#include "Unarchiver.h"

#include "ultra-shared/Config/ZipFolder.h"
#include "ultra-shared/Helpers/FileUtils.h"

#include "archive.h"
#include "archive_entry.h"

//-----------------------------------------------------------------------------

// Counts the regular files while validating every entry against the limits;
// -1 on anything unusable
static int countArchiveFiles ( const juce::MemoryBlock& mb, const Unarchiver::Limits& limits )
{
	auto	arch = archive_read_new ();

	archive_read_support_format_7zip ( arch );

	if ( archive_read_open_memory ( arch, mb.getData (), mb.getSize () ) != ARCHIVE_OK )
	{
		archive_read_free ( arch );
		return -1;
	}

	auto		numTotalFiles = 0;
	uint64_t	totalDeclared = 0;

	archive_entry*	entry;
	while ( archive_read_next_header ( arch, &entry ) == ARCHIVE_OK )
	{
		// Check if the current entry is a regular file, and not a directory
		if ( archive_entry_filetype ( entry ) == AE_IFREG )
		{
			++numTotalFiles;

			// libarchive hands back null when an entry name will not convert to UTF-8
			const auto	pathname = archive_entry_pathname_utf8 ( entry );
			if ( ! pathname )
			{
				Z_ERR ( "Archive entry " << juce::String ( numTotalFiles ) << " has no usable UTF-8 name" );
				archive_read_free ( arch );
				return -1;
			}

			const auto	entrySize = archive_entry_size ( entry );
			if ( entrySize < 0 || uint64_t ( entrySize ) > limits.maxEntrySize )
			{
				Z_ERR ( "Archive entry too large: " << juce::String ( pathname ) << " (" << juce::String ( entrySize ) << " bytes)" );
				archive_read_free ( arch );
				return -1;
			}

			totalDeclared += uint64_t ( entrySize );
		}

		// Skip the entry's data, only the header matters here
		archive_read_data_skip ( arch );
	}

	archive_read_free ( arch );

	if ( totalDeclared > mb.getSize () * limits.maxTotalRatio )
	{
		Z_ERR ( "Archive declares " << juce::String ( totalDeclared ) << " bytes from a " << juce::String ( mb.getSize () ) << " byte archive" );
		return -1;
	}

	return numTotalFiles;
}
//-----------------------------------------------------------------------------

int Unarchiver::extractArchive ( const std::string& dstFolder, const juce::MemoryBlock& mb, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles, juce::Thread* thread, const Limits& limits )
{
	progress = 0.0f;
	files = 0;
	maxFiles = 0;

	// Count files first, for the progress report
	const auto	numTotalFiles = countArchiveFiles ( mb, limits );
	if ( numTotalFiles < 0 )
		return -1;

	maxFiles = numTotalFiles;

	if ( thread && thread->threadShouldExit () )
		return -1;

	auto	arch = archive_read_new ();

	archive_read_support_format_7zip ( arch );

	if ( archive_read_open_memory ( arch, mb.getData (), mb.getSize () ) != ARCHIVE_OK )
	{
		archive_read_free ( arch );
		return -1;
	}

	std::atomic<int>	numFilesExtracted = 0;

	// The saves happen on the pool thread, which cannot abort the read loop itself
	std::atomic<bool>	saveFailed = false;

	std::unordered_set<juce::String>	folders;

	Z_DLOG ( "Extracting " << maxFiles << " files" );

	//
	// Extract each file and save it in a separate thread
	//
	{
		juce::ThreadPool	tp ( 1, 0, juce::Thread::Priority::low );

		archive_entry*	entry;
		int	res;
		while ( ( res = archive_read_next_header ( arch, &entry ) ) == ARCHIVE_OK )
		{
			// Stop at the first unwritable file rather than grinding through 60k more
			if ( saveFailed || ( thread && thread->threadShouldExit () ) )
			{
				archive_read_free ( arch );
				return -1;
			}

			// Skip the entry if it's not a regular file (e.g. directory, symlink, etc.)
			if ( archive_entry_filetype ( entry ) != AE_IFREG )
			{
				archive_read_data_skip ( arch );
				continue;
			}

			// Entry paths must stay inside the destination ("../", absolute paths).
			// The counting pass above has already rejected entries without a usable name
			const auto	dstFile = juce::File ( dstFolder ).getChildFile ( archive_entry_pathname_utf8 ( entry ) );
			if ( ! dstFile.isAChildOf ( juce::File ( dstFolder ) ) )
			{
				Z_ERR ( "Archive entry escapes destination: " << juce::String ( archive_entry_pathname_utf8 ( entry ) ) );
				archive_read_data_skip ( arch );
				continue;
			}

			// Unpack the file data into a buffer
			std::vector<uint8_t>	buffer;
			try
			{
				buffer.resize ( size_t ( archive_entry_size ( entry ) ) );
			}
			catch ( const std::bad_alloc& )
			{
				Z_ERR ( "Out of memory extracting " << juce::String ( archive_entry_pathname_utf8 ( entry ) ) );
				archive_read_free ( arch );
				return -1;
			}

			const auto	r = archive_read_data ( arch, buffer.data (), buffer.size () );
			if ( r != buffer.size () )
			{
				const auto	name = juce::String ( archive_entry_pathname_utf8 ( entry ) );

				// A short read leaves libarchive's error string null
				if ( const auto why = archive_error_string ( arch ) )
				{
					Z_ERR ( "Could not read " << name << " from the archive: " << juce::String ( why ) );
				}
				else
				{
					Z_ERR ( "Short read on " << name << ": " << juce::String ( int64_t ( r ) )
							<< " of " << juce::String ( uint64_t ( buffer.size () ) ) << " bytes" );
				}

				archive_read_free ( arch );
				return -1;
			}

			if ( thread && thread->threadShouldExit () )
			{
				archive_read_free ( arch );
				return -1;
			}

			// Save the file on a separate thread, so the next one unpacks
			// while this one is being written
			tp.addJob ( [ &, saveBuf = std::move ( buffer ), dst = dstFile ]
			{
				if ( auto fpName = dst.getParentDirectory ().getFullPathName (); ! folders.contains ( fpName ) )
				{
					folders.insert ( fpName );
					juce::File ( fpName ).createDirectory ();
				}

				// The caller reads the count as "the tree is complete"
				if ( ! fileutils::replaceFile ( dst, saveBuf.data (), saveBuf.size () ) )
				{
					saveFailed = true;
					return;
				}

				numFilesExtracted.fetch_add ( 1 );
				files.fetch_add ( 1 );
				progress = float ( numFilesExtracted.load () ) / float ( maxFiles );
			} );
		}

		// The jobs only reference their own buffer copies, so the archive
		// can be freed as soon as the read loop is done
		archive_read_free ( arch );

		if ( res != ARCHIVE_EOF )
		{
			tp.removeAllJobs ( true, -1 );
			return -1;
		}

		Z_DLOG ( "Finished extracting, waiting for saves to finish" );

		if ( thread && thread->threadShouldExit () )
		{
			tp.removeAllJobs ( true, -1 );
			return -1;
		}

		while ( tp.getNumJobs () )
		{
			if ( thread && thread->threadShouldExit () )
			{
				tp.removeAllJobs ( true, -1 );
				return -1;
			}

			juce::Thread::sleep ( 20 );
		}

		Z_DLOG ( "All files saved" );
	}

	if ( saveFailed )
		return -1;

	return numFilesExtracted;
}
//-----------------------------------------------------------------------------

int Unarchiver::convertArchiveToZip ( const juce::File& dstZip, const juce::MemoryBlock& mb, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles, juce::Thread* thread, const Limits& limits )
{
	progress = 0.0f;
	files = 0;
	maxFiles = 0;

	const auto	numTotalFiles = countArchiveFiles ( mb, limits );
	if ( numTotalFiles < 0 )
		return -1;

	maxFiles = numTotalFiles;

	if ( thread && thread->threadShouldExit () )
		return -1;

	auto	arch = archive_read_new ();

	archive_read_support_format_7zip ( arch );

	if ( archive_read_open_memory ( arch, mb.getData (), mb.getSize () ) != ARCHIVE_OK )
	{
		archive_read_free ( arch );
		return -1;
	}

	juce::TemporaryFile	temp ( dstZip );
	auto				numConverted = 0;

	Z_DLOG ( "Converting " << maxFiles << " files into " << dstZip.getFullPathName () );

	{
		juce::FileOutputStream	out ( temp.getFile (), 1 << 16 );
		if ( ! out.openedOk () )
		{
			Z_ERR ( "Cannot write " << temp.getFile ().getFullPathName () );
			archive_read_free ( arch );
			return -1;
		}

		ZipFolder::Writer	writer ( out );

		archive_entry*	entry;
		int	res;
		while ( ( res = archive_read_next_header ( arch, &entry ) ) == ARCHIVE_OK )
		{
			if ( thread && thread->threadShouldExit () )
			{
				archive_read_free ( arch );
				return -1;
			}

			if ( archive_entry_filetype ( entry ) != AE_IFREG )
			{
				archive_read_data_skip ( arch );
				continue;
			}

			auto	name = juce::String ( archive_entry_pathname_utf8 ( entry ) ).replaceCharacter ( '\\', '/' );

			// The full archive wraps everything in its C64Music folder
			if ( name.startsWithIgnoreCase ( "C64Music/" ) )
				name = name.substring ( 9 );

			// Hostile names never make it into the archive
			if ( name.isEmpty () || name.startsWithChar ( '/' ) || name.contains ( ".." ) || name.containsChar ( ':' ) )
			{
				Z_ERR ( "Archive entry with unusable name skipped: " << name );
				archive_read_data_skip ( arch );
				continue;
			}

			std::vector<uint8_t>	buffer;
			try
			{
				buffer.resize ( size_t ( archive_entry_size ( entry ) ) );
			}
			catch ( const std::bad_alloc& )
			{
				Z_ERR ( "Out of memory converting " << name );
				archive_read_free ( arch );
				return -1;
			}

			const auto	r = archive_read_data ( arch, buffer.data (), buffer.size () );
			if ( r != buffer.size () )
			{
				// A short read leaves libarchive's error string null
				if ( const auto why = archive_error_string ( arch ) )
				{
					Z_ERR ( "Could not read " << name << " from the archive: " << juce::String ( why ) );
				}
				else
				{
					Z_ERR ( "Short read on " << name << ": " << juce::String ( int64_t ( r ) )
							<< " of " << juce::String ( uint64_t ( buffer.size () ) ) << " bytes" );
				}

				archive_read_free ( arch );
				return -1;
			}

			const auto	modTime = juce::Time ( int64_t ( archive_entry_mtime ( entry ) ) * 1000 );

			if ( ! writer.addFile ( name, buffer.data (), buffer.size (), modTime ) )
			{
				archive_read_free ( arch );
				return -1;
			}

			++numConverted;
			files.fetch_add ( 1 );
			progress = float ( numConverted ) / float ( maxFiles );
		}

		archive_read_free ( arch );

		if ( res != ARCHIVE_EOF || ( thread && thread->threadShouldExit () ) )
			return -1;

		if ( ! writer.finish () )
			return -1;

		out.flush ();

		if ( ! out.getStatus ().wasOk () )
		{
			Z_ERR ( "Write failed: " << out.getStatus ().getErrorMessage () );
			return -1;
		}
	}

	if ( thread && thread->threadShouldExit () )
		return -1;

	if ( ! temp.overwriteTargetFileWithTemporary () )
	{
		Z_ERR ( "Cannot replace " << dstZip.getFullPathName () );
		return -1;
	}

	Z_DLOG ( "Conversion finished, " << numConverted << " files" );

	return numConverted;
}
//-----------------------------------------------------------------------------

int Unarchiver::extractArchiveInto ( ZipFolder& zip, const juce::String& pathPrefix, const juce::MemoryBlock& mb, std::atomic<float>& progress, std::atomic<int>& files, std::atomic<int>& maxFiles, const Limits& limits )
{
	progress = 0.0f;
	files = 0;
	maxFiles = 0;

	const auto	numTotalFiles = countArchiveFiles ( mb, limits );
	if ( numTotalFiles < 0 )
		return -1;

	maxFiles = numTotalFiles;

	auto	arch = archive_read_new ();

	archive_read_support_format_7zip ( arch );

	if ( archive_read_open_memory ( arch, mb.getData (), mb.getSize () ) != ARCHIVE_OK )
	{
		archive_read_free ( arch );
		return -1;
	}

	auto	numStaged = 0;

	archive_entry*	entry;
	int	res;
	while ( ( res = archive_read_next_header ( arch, &entry ) ) == ARCHIVE_OK )
	{
		if ( archive_entry_filetype ( entry ) != AE_IFREG )
		{
			archive_read_data_skip ( arch );
			continue;
		}

		const auto	name = juce::String ( archive_entry_pathname_utf8 ( entry ) ).replaceCharacter ( '\\', '/' );

		// Hostile names never make it into the archive
		if ( name.isEmpty () || name.startsWithChar ( '/' ) || name.contains ( ".." ) || name.containsChar ( ':' ) )
		{
			Z_ERR ( "Archive entry with unusable name skipped: " << name );
			archive_read_data_skip ( arch );
			continue;
		}

		std::vector<uint8_t>	buffer;
		try
		{
			buffer.resize ( size_t ( archive_entry_size ( entry ) ) );
		}
		catch ( const std::bad_alloc& )
		{
			Z_ERR ( "Out of memory extracting " << name );
			archive_read_free ( arch );
			return -1;
		}

		const auto	r = archive_read_data ( arch, buffer.data (), buffer.size () );
		if ( r != buffer.size () )
		{
			// A short read leaves libarchive's error string null
			if ( const auto why = archive_error_string ( arch ) )
			{
				Z_ERR ( "Could not read " << name << " from the archive: " << juce::String ( why ) );
			}
			else
			{
				Z_ERR ( "Short read on " << name << ": " << juce::String ( int64_t ( r ) )
						<< " of " << juce::String ( uint64_t ( buffer.size () ) ) << " bytes" );
			}

			archive_read_free ( arch );
			return -1;
		}

		zip.writeFile ( pathPrefix + name, buffer.data (), buffer.size () );

		++numStaged;
		files.fetch_add ( 1 );
		progress = float ( numStaged ) / float ( maxFiles );
	}

	archive_read_free ( arch );

	return res == ARCHIVE_EOF ? numStaged : -1;
}
//-----------------------------------------------------------------------------
