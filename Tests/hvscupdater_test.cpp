// HVSCUpdater dual-tree regression: the same synthetic update script runs
// against a loose folder (FolderTree) and a zip archive (ZipTree); the
// resulting collections must match file for file, byte for byte. Also proves
// the zip stays untouched on disk when a script fails.
//
// Build: cmake --build --preset vs --config Release --target hvscupdater_test

#include <JuceHeader.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "ultra-shared/Config/ZipFolder.h"

#include "Database/HVSCUpdater/HVSCUpdater.h"

//-----------------------------------------------------------------------------

static int failures = 0;

static void check ( const bool ok, const juce::String& what )
{
	if ( ! ok )
	{
		std::printf ( "FAIL: %s\n", what.toRawUTF8 () );
		++failures;
	}
}
//-----------------------------------------------------------------------------

static std::string lower ( const juce::String& path )
{
	return path.toLowerCase ().toStdString ();
}
//-----------------------------------------------------------------------------

// A minimal but valid PSID v2 file: 0x7C header + embedded load address +
// a token payload
static juce::MemoryBlock makePSID ( const juce::String& title, const juce::String& author, const juce::String& released )
{
	uint8_t	h[ 0x7C + 6 ] = {};

	std::memcpy ( h, "PSID", 4 );
	h[ 0x05 ] = 2;		// version 2
	h[ 0x07 ] = 0x7C;	// data offset
	h[ 0x0A ] = 0x10;	// init $1000
	h[ 0x0C ] = 0x10;	// play $1003
	h[ 0x0D ] = 0x03;
	h[ 0x0F ] = 1;		// songs
	h[ 0x11 ] = 1;		// start song

	std::strncpy ( reinterpret_cast<char*> ( h + 0x16 ), title.toRawUTF8 (), 31 );
	std::strncpy ( reinterpret_cast<char*> ( h + 0x36 ), author.toRawUTF8 (), 31 );
	std::strncpy ( reinterpret_cast<char*> ( h + 0x56 ), released.toRawUTF8 (), 31 );

	// Embedded load address $1000 plus four bytes of "code"
	h[ 0x7C ] = 0x00;
	h[ 0x7D ] = 0x10;
	h[ 0x7E ] = 0x60;
	h[ 0x7F ] = 0xEA;
	h[ 0x80 ] = 0xEA;
	h[ 0x81 ] = 0x60;

	return { h, sizeof ( h ) };
}
//-----------------------------------------------------------------------------

using Content = std::map<std::string, std::pair<juce::String, juce::MemoryBlock>>;

static void add ( Content& c, const juce::String& path, juce::MemoryBlock data )
{
	c[ lower ( path ) ] = { path, std::move ( data ) };
}
//-----------------------------------------------------------------------------

// The base collection plus the extracted update tree, identical for both runs
static Content baseContent ( const bool withUpdate )
{
	Content	c;

	add ( c, "DOCUMENTS/placeholder.txt", juce::MemoryBlock ( "docs", 4 ) );
	add ( c, "MUSICIANS/A/First.sid", makePSID ( "Old First", "A. Author", "1987 A" ) );
	add ( c, "MUSICIANS/B/Second.sid", makePSID ( "Second", "B. Author", "1988 B" ) );
	add ( c, "GAMES/Old.sid", makePSID ( "Old Game", "G. Author", "1985 G" ) );
	add ( c, "DEMOS/Nest/DirA/x.sid", makePSID ( "X", "XA", "1990" ) );
	add ( c, "DEMOS/Nest/DirA/y.sid", makePSID ( "Y", "YA", "1991" ) );
	add ( c, "DEMOS/DirB/z.sid", makePSID ( "Z", "ZA", "1992" ) );

	if ( ! withUpdate )
		return c;

	// Deliberate wrong-case script paths exercise the case correction
	const juce::String	script =
		"; synthetic test update\n"
		"TITLE\n"
		"/musicians/a/First.sid\n"
		"New First Title\n"
		"AUTHOR\n"
		"/MUSICIANS/B/second.sid\n"
		"New Author\n"
		"DELETE\n"
		"/GAMES/Old.sid\n"
		"MOVE\n"
		"/update/New.sid\n"
		"/MUSICIANS/A/\n"
		"MOVE\n"
		"/DEMOS/Nest/DirA/\n"
		"/DEMOS/DirB/\n"
		"DELETE\n"
		"/DEMOS/Nest/DirA/\n"
		"/DEMOS/Nest/\n"
		"REPLACE\n"
		"/update/Replacement.sid\n"
		"/DEMOS/DirB/z.sid\n";

	add ( c, "update/Update99.hvs", juce::MemoryBlock ( script.toRawUTF8 (), script.getNumBytesAsUTF8 () ) );
	add ( c, "update/New.sid", makePSID ( "New", "N. Author", "2026" ) );
	add ( c, "update/Replacement.sid", makePSID ( "Replacement", "R. Author", "2026" ) );

	return c;
}
//-----------------------------------------------------------------------------

static Content folderSnapshot ( const juce::File& root )
{
	const auto	rootLen = root.getFullPathName ().length () + 1;

	Content	c;

	for ( const auto& entry : juce::RangedDirectoryIterator ( root, true, "*", juce::File::findFiles ) )
	{
		const auto	rel = entry.getFile ().getFullPathName ().substring ( rootLen ).replaceCharacter ( '\\', '/' );

		juce::MemoryBlock	mb;
		entry.getFile ().loadFileAsData ( mb );
		add ( c, rel, std::move ( mb ) );
	}

	return c;
}
//-----------------------------------------------------------------------------

static Content zipSnapshot ( ZipFolder& zip )
{
	Content	c;

	for ( const auto& rel : zip.listFiles ( {}, true ) )
		add ( c, rel, zip.load ( rel ) );

	return c;
}
//-----------------------------------------------------------------------------

static void compareContent ( const Content& a, const Content& b, const juce::String& label )
{
	check ( a.size () == b.size (), label + ": file count " + juce::String ( int ( a.size () ) ) + " vs " + juce::String ( int ( b.size () ) ) );

	for ( const auto& [ key, f ] : a )
	{
		const auto	it = b.find ( key );

		if ( it == b.end () )
			check ( false, label + ": missing " + f.first );
		else
			check ( it->second.second == f.second, label + ": content differs for " + f.first );
	}

	for ( const auto& [ key, f ] : b )
		if ( ! a.contains ( key ) )
			check ( false, label + ": stray " + f.first );
}
//-----------------------------------------------------------------------------

// Applies extracted real updates onto a zip collection, staged as one chain
// with a single commit at the end:
// --real <collection.zip> <extracted-update-root> <version> [<root> <ver> ...]
static int realMode ( const char* zipPath, const char* const* pairs, const int numPairs )
{
	ZipFolder	zip;

	if ( ! zip.open ( juce::File ( zipPath ) ) )
	{
		std::printf ( "FAIL: cannot open %s\n", zipPath );
		return 1;
	}

	const auto	prefix = zip.exists ( "DOCUMENTS/HVSC.txt" ) ? juce::String () : juce::String ( "C64Music/" );

	ZipTree	tree ( zip, prefix );

	for ( auto p = 0; p < numPairs; ++p )
	{
		const juce::File	updateDir ( pairs[ p * 2 ] );
		const auto			version = juce::String ( pairs[ p * 2 + 1 ] ).getIntValue ();
		const auto			rootLen = updateDir.getFullPathName ().length () + 1;
		auto				staged = 0;

		for ( const auto& entry : juce::RangedDirectoryIterator ( updateDir, true, "*", juce::File::findFiles ) )
		{
			const auto	rel = entry.getFile ().getFullPathName ().substring ( rootLen ).replaceCharacter ( '\\', '/' );

			juce::MemoryBlock	mb;
			entry.getFile ().loadFileAsData ( mb );
			zip.writeFile ( prefix + rel, mb );
			++staged;
		}

		std::atomic<float>	progress = 0.0f;
		std::atomic<int>	files = 0, maxFiles = 0;

		const auto	errors = HVSCUpdater ().update ( tree, version, progress, files, maxFiles );

		std::printf ( "update %d: %d files staged, %d errors\n", version, staged, errors );

		if ( errors != 0 )
			return 1;
	}

	if ( ! tree.finish () )
	{
		std::printf ( "FAIL: commit failed\n" );
		return 1;
	}

	std::printf ( "committed once after %d update(s)\n", numPairs );
	return 0;
}
//-----------------------------------------------------------------------------

// Compares two zip collections by entry CRC and size:
// --compare <a.zip> <b.zip>
static int compareMode ( const char* aPath, const char* bPath )
{
	PakFile	a, b;

	if ( ! a.open ( juce::File ( aPath ) ) || ! b.open ( juce::File ( bPath ) ) )
	{
		std::printf ( "FAIL: cannot open archives\n" );
		return 1;
	}

	struct Ref { juce::String path; uint32_t crc; int64_t size; };
	std::map<std::string, Ref>	bMap;

	for ( const auto& e : b.getEntries () )
		bMap[ lower ( e.path ) ] = { e.path, e.crc, e.uncompressedSize };

	auto	mismatches = 0;

	for ( const auto& e : a.getEntries () )
	{
		const auto	it = bMap.find ( lower ( e.path ) );

		if ( it == bMap.end () )
		{
			std::printf ( "only in A: %s\n", e.path.toRawUTF8 () );
			++mismatches;
			continue;
		}

		if ( it->second.crc != e.crc || it->second.size != e.uncompressedSize )
		{
			std::printf ( "differs:   %s\n", e.path.toRawUTF8 () );
			++mismatches;
		}

		bMap.erase ( it );
	}

	for ( const auto& [ key, r ] : bMap )
	{
		std::printf ( "only in B: %s\n", r.path.toRawUTF8 () );
		++mismatches;
	}

	std::printf ( "%d entries in A, %d mismatches\n", a.getNumEntries (), mismatches );
	return mismatches == 0 ? 0 : 1;
}
//-----------------------------------------------------------------------------

struct StdoutLogger final : juce::Logger
{
	void logMessage ( const juce::String& m ) override	{	std::printf ( "%s\n", m.toRawUTF8 () );	}
};

static StdoutLogger stdoutLogger;

//-----------------------------------------------------------------------------

int main ( int argc, char* argv[] )
{
	juce::Logger::setCurrentLogger ( &stdoutLogger );
	const juce::ScopeGuard	resetLogger { [] { juce::Logger::setCurrentLogger ( nullptr ); } };

	if ( argc >= 5 && ( argc - 3 ) % 2 == 0 && juce::String ( argv[ 1 ] ) == "--real" )
		return realMode ( argv[ 2 ], argv + 3, ( argc - 3 ) / 2 );

	if ( argc == 4 && juce::String ( argv[ 1 ] ) == "--compare" )
		return compareMode ( argv[ 2 ], argv[ 3 ] );

	const auto	dir = juce::File::getSpecialLocation ( juce::File::tempDirectory ).getChildFile ( "hvscupdater_test" );
	dir.deleteRecursively ();
	dir.createDirectory ();

	std::atomic<float>	progress = 0.0f;
	std::atomic<int>	files = 0, maxFiles = 0;

	const auto	content = baseContent ( true );

	//
	// Folder run
	//
	const auto	folderRoot = dir.getChildFile ( "C64Music" );

	for ( const auto& [ key, f ] : content )
	{
		const auto	dst = folderRoot.getChildFile ( f.first );
		dst.getParentDirectory ().createDirectory ();
		check ( dst.replaceWithData ( f.second.getData (), f.second.getSize () ), "seed " + f.first );
	}

	{
		FolderTree	tree ( folderRoot );
		check ( HVSCUpdater ().update ( tree, 99, progress, files, maxFiles ) == 0, "folder update clean" );
	}

	const auto	folderResult = folderSnapshot ( folderRoot );

	//
	// Zip run: same base entries in the archive, the update tree staged like
	// extractArchiveInto does
	//
	const auto	zipFile = dir.getChildFile ( "C64Music.zip" );

	{
		juce::FileOutputStream	out ( zipFile, 1 << 16 );
		check ( out.openedOk (), "create zip" );

		ZipFolder::Writer	writer ( out );

		for ( const auto& [ key, f ] : content )
			if ( ! f.first.startsWith ( "update/" ) )
				check ( writer.addFile ( f.first, f.second.getData (), f.second.getSize (), juce::Time::getCurrentTime () ), "zip seed " + f.first );

		check ( writer.finish (), "zip seed finish" );
		out.flush ();
		check ( out.getStatus ().wasOk (), "zip seed status" );
	}

	ZipFolder	zip;
	check ( zip.open ( zipFile ), "zip open" );

	for ( const auto& [ key, f ] : content )
		if ( f.first.startsWith ( "update/" ) )
			zip.writeFile ( f.first, f.second );

	{
		ZipTree	tree ( zip, {} );
		check ( HVSCUpdater ().update ( tree, 99, progress, files, maxFiles ) == 0, "zip update clean" );
		check ( zip.hasPendingChanges (), "update stays staged until finish" );
		check ( tree.finish (), "zip commit" );
	}

	check ( ! zip.hasPendingChanges (), "zip update committed" );

	//
	// The two results must be identical, and the committed state must be what
	// a fresh reader sees on disk
	//
	compareContent ( folderResult, zipSnapshot ( zip ), "folder vs zip" );

	{
		ZipFolder	reopened;
		check ( reopened.open ( zipFile ), "zip reopen" );
		compareContent ( folderResult, zipSnapshot ( reopened ), "folder vs reopened zip" );
	}

	// Spot checks on the shared result
	{
		check ( folderResult.contains ( lower ( "DOCUMENTS/Update99.hvs" ) ), "applied marker moved" );
		check ( ! folderResult.contains ( lower ( "GAMES/Old.sid" ) ), "deleted tune gone" );
		check ( folderResult.contains ( lower ( "MUSICIANS/A/New.sid" ) ), "new tune moved in" );
		check ( folderResult.contains ( lower ( "DEMOS/DirB/x.sid" ) ) && folderResult.contains ( lower ( "DEMOS/DirB/y.sid" ) ), "folder merge" );

		for ( const auto& [ key, f ] : folderResult )
			check ( ! f.first.startsWith ( "update/" ), "update tree cleaned: " + f.first );

		const auto	it = folderResult.find ( lower ( "MUSICIANS/A/First.sid" ) );
		check ( it != folderResult.end ()
				&& juce::String ( juce::CharPointer_UTF8 ( static_cast<const char*> ( it->second.second.getData () ) + 0x16 ) ) == "New First Title",
				"title rewritten" );

		const auto	z = folderResult.find ( lower ( "DEMOS/DirB/z.sid" ) );
		check ( z != folderResult.end ()
				&& juce::String ( juce::CharPointer_UTF8 ( static_cast<const char*> ( z->second.second.getData () ) + 0x16 ) ) == "Replacement",
				"replace applied" );
	}

	//
	// Failing script: the zip on disk must stay exactly as it was
	//
	const auto	failZip = dir.getChildFile ( "fail.zip" );

	{
		juce::FileOutputStream	out ( failZip, 1 << 16 );
		ZipFolder::Writer		writer ( out );

		for ( const auto& [ key, f ] : baseContent ( false ) )
			check ( writer.addFile ( f.first, f.second.getData (), f.second.getSize (), juce::Time::getCurrentTime () ), "failzip seed" );

		check ( writer.finish (), "failzip finish" );
	}

	{
		ZipFolder	fz;
		check ( fz.open ( failZip ), "failzip open" );

		const auto	before = zipSnapshot ( fz );

		const juce::String	badScript = "TITLE\n/MUSICIANS/A/Missing.sid\nNope\n";
		fz.writeFile ( "update/Update99.hvs", badScript.toRawUTF8 (), badScript.getNumBytesAsUTF8 () );

		ZipTree	tree ( fz, {} );
		check ( HVSCUpdater ().update ( tree, 99, progress, files, maxFiles ) > 0, "bad script reports errors" );

		// What the installer does on errors
		fz.discardPendingChanges ();
		compareContent ( before, zipSnapshot ( fz ), "failed update leaves overlay clean" );

		ZipFolder	reopened;
		check ( reopened.open ( failZip ), "failzip reopen" );
		compareContent ( before, zipSnapshot ( reopened ), "failed update leaves disk untouched" );
	}

	if ( failures != 0 )
	{
		std::printf ( "FAILED: %d check(s)\n", failures );
		return 1;
	}

	std::printf ( "PASS\n" );
	return 0;
}
//-----------------------------------------------------------------------------
