// ZipFolder round-trip: Writer-built archives, staged mutations, commit
// rewrites and zip64 entry counts, cross-checked against juce::ZipFile and an
// independent bitwise CRC-32.
//
// Build: cmake --build --preset vs --config Release --target zipfolder_test

#include <JuceHeader.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

#include "ultra-shared/Config/ZipFolder.h"

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

// Bitwise reference CRC-32, deliberately table-free so it shares nothing with
// the implementation under test
static uint32_t refCrc ( const juce::MemoryBlock& mb )
{
	auto		crc = 0xFFFFFFFFu;
	const auto*	p = static_cast<const uint8_t*> ( mb.getData () );

	for ( size_t i = 0; i < mb.getSize (); ++i )
	{
		crc ^= p[ i ];
		for ( auto k = 0; k < 8; ++k )
			crc = ( crc & 1 ) != 0 ? 0xEDB88320u ^ ( crc >> 1 ) : crc >> 1;
	}

	return crc ^ 0xFFFFFFFFu;
}
//-----------------------------------------------------------------------------

struct ModelFile
{
	juce::String		path;
	juce::MemoryBlock	data;
};

using Model = std::map<std::string, ModelFile>;

//-----------------------------------------------------------------------------

static void verify ( const ZipFolder& zip, const Model& model, const juce::String& label )
{
	const auto	listed = zip.listFiles ( "", true );

	check ( listed.size () == int ( model.size () ), label + ": file count " + juce::String ( listed.size () ) + " vs " + juce::String ( int ( model.size () ) ) );

	for ( const auto& [ key, m ] : model )
	{
		check ( zip.exists ( m.path ), label + ": missing " + m.path );
		check ( zip.load ( m.path ) == m.data, label + ": content " + m.path );
	}

	for ( const auto& name : listed )
	{
		const auto	it = model.find ( lower ( name ) );

		if ( it == model.end () )
			check ( false, label + ": stray " + name );
		else
			check ( it->second.path == name, label + ": case of " + name );
	}
}
//-----------------------------------------------------------------------------

static void crossCheck ( const juce::File& file, const Model& model, const juce::String& label )
{
	juce::ZipFile	zf ( file );

	check ( zf.getNumEntries () == int ( model.size () ), label + ": juce entry count" );

	for ( auto i = 0; i < zf.getNumEntries (); ++i )
	{
		const auto*	e = zf.getEntry ( i );
		const auto	it = model.find ( lower ( e->filename ) );

		if ( it == model.end () )
		{
			check ( false, label + ": juce stray " + e->filename );
			continue;
		}

		std::unique_ptr<juce::InputStream>	in ( zf.createStreamForEntry ( i ) );
		juce::MemoryBlock					mb;

		if ( in != nullptr )
			in->readIntoMemoryBlock ( mb );

		check ( in != nullptr && mb == it->second.data, label + ": juce content " + e->filename );
	}
}
//-----------------------------------------------------------------------------

int main ()
{
	const auto	dir = juce::File::getSpecialLocation ( juce::File::tempDirectory ).getChildFile ( "zipfolder_test" );
	dir.deleteRecursively ();
	dir.createDirectory ();

	const auto	zipFile = dir.getChildFile ( "test.zip" );
	const auto	now = juce::Time::getCurrentTime ();

	juce::Random	rng ( 20260829 );
	Model			model;

	auto	addModel = [ &model ] ( const juce::String& path, juce::MemoryBlock data )
	{
		model[ lower ( path ) ] = { path, std::move ( data ) };
	};

	auto	randomBlock = [ &rng ] ( const int numBytes )
	{
		juce::MemoryBlock	mb ( static_cast<size_t> ( numBytes ) );
		auto*				p = static_cast<uint8_t*> ( mb.getData () );

		for ( auto i = 0; i < numBytes; ++i )
			p[ i ] = uint8_t ( rng.nextInt ( 256 ) );

		return mb;
	};

	auto	textBlock = [] ( const juce::String& line, const int repeats )
	{
		juce::MemoryOutputStream	mo;

		for ( auto i = 0; i < repeats; ++i )
			mo << line << "\n";

		return mo.getMemoryBlock ();
	};

	addModel ( "DEMOS/A-F/Alpha.sid", randomBlock ( 4000 ) );
	addModel ( "DEMOS/A-F/Beta.sid", randomBlock ( 2500 ) );
	addModel ( "DOCUMENTS/Songlengths.md5", textBlock ( "; /MUSICIANS/X/test.sid  0:42", 40 ) );
	addModel ( "DOCUMENTS/STIL.txt", textBlock ( "COMMENT: the same line over and over, deflate food", 4000 ) );
	addModel ( "empty.bin", {} );
	addModel ( "random.bin", randomBlock ( 65536 ) );
	addModel ( "crc.bin", juce::MemoryBlock ( "123456789", 9 ) );

	const auto	utfName = juce::String::fromUTF8 ( "H\xc3\xbclsbeck/\xc3\x9cn\xc3\xaf" "code \xc3\xb1" "ame.txt" );
	addModel ( utfName, textBlock ( "utf-8 payload", 3 ) );

	for ( auto i = 0; i < 30; ++i )
	{
		const auto	path = "MUSICIANS/" + juce::String::charToString ( 'A' + i % 5 ) + "/tune_" + juce::String ( i ) + ".sid";
		addModel ( path, ( i & 1 ) != 0 ? randomBlock ( 1000 + i * 37 ) : textBlock ( "SIDDATA " + juce::String ( i ), 200 ) );
	}

	//
	// Build the archive with the Writer, then read it back three ways
	//
	{
		juce::FileOutputStream	out ( zipFile, 1 << 16 );
		check ( out.openedOk (), "create zip" );

		ZipFolder::Writer	writer ( out );

		for ( const auto& [ key, m ] : model )
			check ( writer.addFile ( m.path, m.data.getData (), m.data.getSize (), now ), "addFile " + m.path );

		check ( writer.finish (), "finish" );
		out.flush ();
		check ( out.getStatus ().wasOk (), "write status" );
	}

	ZipFolder	zip;
	check ( zip.open ( zipFile ), "open" );
	verify ( zip, model, "initial" );
	crossCheck ( zipFile, model, "initial" );

	{
		PakFile	pak;
		check ( pak.open ( zipFile ), "pak open" );

		const auto*	e = pak.findEntry ( "crc.bin" );
		check ( e != nullptr && e->crc == 0xCBF43926u, "crc known vector" );

		const auto*	stil = pak.findEntry ( "DOCUMENTS/STIL.txt" );
		check ( stil != nullptr && stil->deflated, "compressible entry deflated" );
		check ( stil != nullptr && stil->crc == refCrc ( model[ lower ( "DOCUMENTS/STIL.txt" ) ].data ), "crc matches reference impl" );
		check ( stil != nullptr && stil->compressedSize < stil->uncompressedSize / 10, "deflate actually compressed" );

		const auto*	rnd = pak.findEntry ( "random.bin" );
		check ( rnd != nullptr && ! rnd->deflated, "incompressible entry stored" );
	}

	check ( zip.folderExists ( "MUSICIANS" ), "folderExists MUSICIANS" );
	check ( zip.folderExists ( "DEMOS/A-F" ), "folderExists DEMOS/A-F" );
	check ( ! zip.folderExists ( "NOPE" ), "folderExists NOPE" );
	check ( zip.listFiles ( "DEMOS/A-F", false ).size () == 2, "non-recursive list" );
	check ( zip.listFiles ( "", true, "*.sid" ).size () == 32, "wildcard list" );

	{
		const auto	folders = zip.listFolders ( "" );
		check ( folders.contains ( "MUSICIANS" ) && folders.contains ( "DEMOS" ) && folders.contains ( "DOCUMENTS" ), "root folders" );
	}

	{
		auto				stream = zip.createStream ( "DEMOS/A-F/Alpha.sid" );
		juce::MemoryBlock	mb;

		if ( stream != nullptr )
			stream->readIntoMemoryBlock ( mb );

		check ( stream != nullptr && mb == zip.load ( "DEMOS/A-F/Alpha.sid" ), "stream matches load" );
	}

	//
	// Round 1: stage every mutation kind, verify the merged view before and
	// after commit
	//
	auto	moveModel = [ &model ] ( const juce::String& from, const juce::String& to )
	{
		auto	m = std::move ( model[ lower ( from ) ] );
		model.erase ( lower ( from ) );
		m.path = to;
		model[ lower ( to ) ] = std::move ( m );
	};

	const juce::MemoryBlock	newStil ( "NEW STIL", 8 );
	zip.writeFile ( "DOCUMENTS/STIL.txt", newStil );
	model[ lower ( "DOCUMENTS/STIL.txt" ) ].data = newStil;

	const juce::MemoryBlock	bugList ( "BUGS", 4 );
	zip.writeFile ( "DOCUMENTS/BUGlist.txt", bugList );
	addModel ( "DOCUMENTS/BUGlist.txt", bugList );

	check ( zip.rename ( "DEMOS/A-F/Alpha.sid", "DEMOS/G-M/Alpha.sid" ), "rename" );
	moveModel ( "DEMOS/A-F/Alpha.sid", "DEMOS/G-M/Alpha.sid" );

	check ( zip.rename ( "empty.bin", "Empty.BIN" ), "case-only rename" );
	moveModel ( "empty.bin", "Empty.BIN" );

	check ( zip.remove ( "DEMOS/A-F/Beta.sid" ), "remove" );
	model.erase ( lower ( "DEMOS/A-F/Beta.sid" ) );

	check ( ! zip.remove ( "DEMOS/A-F/Beta.sid" ), "double remove refused" );
	check ( ! zip.rename ( "DEMOS/A-F/Beta.sid", "x.sid" ), "rename of removed refused" );
	check ( ! zip.rename ( "not/there.sid", "x.sid" ), "rename of missing refused" );
	check ( zip.load ( "DEMOS/A-F/Beta.sid" ).isEmpty (), "removed loads empty" );

	check ( zip.removeFolder ( "MUSICIANS/A" ), "removeFolder" );
	for ( auto it = model.begin (); it != model.end (); )
		it = it->first.starts_with ( "musicians/a/" ) ? model.erase ( it ) : std::next ( it );

	check ( ! zip.folderExists ( "MUSICIANS/A" ), "removed folder gone" );
	check ( ! zip.listFolders ( "MUSICIANS" ).contains ( "A" ), "removed folder unlisted" );
	check ( zip.exists ( "DEMOS/G-M/Alpha.sid" ) && ! zip.exists ( "DEMOS/A-F/Alpha.sid" ), "rename staged" );
	check ( zip.hasPendingChanges (), "changes pending" );

	// Staged content through streams, and folders that exist only as staged
	{
		auto				renamedStream = zip.createStream ( "DEMOS/G-M/Alpha.sid" );
		auto				stagedStream = zip.createStream ( "DOCUMENTS/STIL.txt" );
		juce::MemoryBlock	renamedData, stagedData;

		if ( renamedStream != nullptr )
			renamedStream->readIntoMemoryBlock ( renamedData );

		if ( stagedStream != nullptr )
			stagedStream->readIntoMemoryBlock ( stagedData );

		check ( renamedData == model[ lower ( "DEMOS/G-M/Alpha.sid" ) ].data, "stream of staged rename" );
		check ( stagedData == newStil, "stream of staged write" );
		check ( zip.createStream ( "DEMOS/A-F/Beta.sid" ) == nullptr, "stream of removed is null" );
		check ( zip.folderExists ( "DEMOS/G-M" ), "staged-only folder exists" );
		check ( zip.listFolders ( "DEMOS" ).contains ( "G-M" ), "staged-only folder listed" );
	}

	verify ( zip, model, "staged" );

	check ( zip.commit (), "commit" );
	check ( ! zip.hasPendingChanges (), "no changes after commit" );
	verify ( zip, model, "committed" );
	crossCheck ( zipFile, model, "committed" );

	{
		ZipFolder	reopened;
		check ( reopened.open ( zipFile ), "reopen" );
		verify ( reopened, model, "reopened" );
	}

	//
	// Round 2 on the committed archive: move a raw-copied entry back, rename
	// onto an existing name, drop the utf-8 entry
	//
	check ( zip.rename ( "DEMOS/G-M/Alpha.sid", "DEMOS/A-F/Alpha.sid" ), "rename back" );
	moveModel ( "DEMOS/G-M/Alpha.sid", "DEMOS/A-F/Alpha.sid" );

	check ( zip.rename ( "DOCUMENTS/Songlengths.md5", "DOCUMENTS/STIL.txt" ), "rename onto existing" );
	moveModel ( "DOCUMENTS/Songlengths.md5", "DOCUMENTS/STIL.txt" );

	check ( zip.remove ( utfName ), "remove utf-8 entry" );
	model.erase ( lower ( utfName ) );

	verify ( zip, model, "staged 2" );
	check ( zip.commit (), "commit 2" );
	verify ( zip, model, "committed 2" );
	crossCheck ( zipFile, model, "committed 2" );

	//
	// Round 3: rename chain, then a fresh file under the vacated name
	//
	check ( zip.rename ( "DEMOS/A-F/Alpha.sid", "chain/one.sid" ), "chain first rename" );
	check ( zip.rename ( "chain/one.sid", "chain/two.sid" ), "chain second rename" );
	moveModel ( "DEMOS/A-F/Alpha.sid", "chain/two.sid" );

	const juce::MemoryBlock	freshAlpha ( "FRESH", 5 );
	zip.writeFile ( "DEMOS/A-F/Alpha.sid", freshAlpha );
	addModel ( "DEMOS/A-F/Alpha.sid", freshAlpha );

	check ( ! zip.exists ( "chain/one.sid" ), "chain middle gone" );
	verify ( zip, model, "staged 3" );
	check ( zip.commit (), "commit 3" );
	verify ( zip, model, "committed 3" );
	crossCheck ( zipFile, model, "committed 3" );

	check ( zip.commit (), "no-op commit" );
	check ( ! zip.hasPendingChanges (), "no-op commit stays clean" );

	//
	// Discard
	//
	const juce::MemoryBlock	x ( "X", 1 );
	zip.writeFile ( "x.txt", x );
	check ( zip.exists ( "x.txt" ), "staged write visible" );
	zip.discardPendingChanges ();
	check ( ! zip.exists ( "x.txt" ) && ! zip.hasPendingChanges (), "discard" );

	//
	// zip64: 70000 entries force the 64-bit end records; a commit on top
	// exercises the raw-copy path across them
	//
	const auto	bigFile = dir.getChildFile ( "big.zip" );

	{
		juce::FileOutputStream	out ( bigFile, 1 << 16 );
		check ( out.openedOk (), "create big zip" );

		ZipFolder::Writer	writer ( out );
		auto				ok = true;

		for ( int64_t i = 0; i < 70000 && ok; ++i )
			ok = writer.addFile ( "n/" + juce::String ( i ) + ".bin", &i, 8, now, 0 );

		check ( ok, "big addFile" );
		check ( writer.finish (), "big finish" );
		out.flush ();
		check ( out.getStatus ().wasOk (), "big write status" );
	}

	{
		ZipFolder	big;
		check ( big.open ( bigFile ), "big open (zip64)" );
		check ( big.listFiles ( "", true ).size () == 70000, "big count" );

		for ( const int64_t i : { int64_t ( 0 ), int64_t ( 65534 ), int64_t ( 65535 ), int64_t ( 69999 ) } )
			check ( big.load ( "n/" + juce::String ( i ) + ".bin" ) == juce::MemoryBlock ( &i, 8 ), "big content " + juce::String ( i ) );

		const juce::MemoryBlock	nine ( "NINE!", 5 );
		big.writeFile ( "n/9.bin", nine );
		check ( big.remove ( "n/10.bin" ), "big remove" );
		check ( big.commit (), "big commit" );
		check ( big.listFiles ( "", true ).size () == 69999, "big count after commit" );
		check ( big.load ( "n/9.bin" ) == nine, "big replaced content" );

		const int64_t	last = 69999;
		check ( big.load ( "n/69999.bin" ) == juce::MemoryBlock ( &last, 8 ), "big raw-copied content" );
	}

	//
	// Negatives
	//
	{
		ZipFolder	nope;
		check ( ! nope.open ( dir.getChildFile ( "missing.zip" ) ), "missing zip refused" );
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
