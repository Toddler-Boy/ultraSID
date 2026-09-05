// hvsc_update brings the zip collection to a newer HVSC release with the
// app's own update chain: every archive stages into the zip, its script runs,
// and one commit writes the result. Any failure leaves the zip untouched.
//
// Usage: hvsc_update <HVSC.zip> [<HVSC_Update_N.7z> ...]
//   Archives in release order, starting at the installed release + 1. With
//   the zip alone it only prints "Release N".
// Build: cmake --build --preset vs --config Release --target hvsc_update

#include <JuceHeader.h>

#include <atomic>
#include <cstdio>

#include "ultra-shared/Config/ZipFolder.h"

#include "Config/HVSCSource.h"
#include "Database/HVSCUpdater/HVSCTree.h"
#include "Database/HVSCUpdater/HVSCUpdater.h"
#include "Helpers/Unarchiver.h"

//-----------------------------------------------------------------------------

// Same caps as the app's installer
constexpr Unarchiver::Limits	hvscLimits { .maxEntrySize = 16ull << 20, .maxTotalRatio = 8 };

struct StdoutLogger final : juce::Logger
{
	void logMessage ( const juce::String& m ) override	{	std::printf ( "%s\n", m.toRawUTF8 () );	}
};

static StdoutLogger stdoutLogger;

//-----------------------------------------------------------------------------

static int installedRelease ()
{
	const auto	lines = juce::StringArray::fromLines ( hvscsource::loadText ( "DOCUMENTS/HVSC.txt" ) );

	for ( const auto& line : lines )
		if ( line.contains ( "Release " ) )
			return line.getTrailingIntValue ();

	return 0;
}
//-----------------------------------------------------------------------------

static juce::File fileArg ( const char* arg )
{
	return juce::File::getCurrentWorkingDirectory ().getChildFile ( arg );
}
//-----------------------------------------------------------------------------

int main ( int argc, char* argv[] )
{
	juce::Logger::setCurrentLogger ( &stdoutLogger );
	const juce::ScopeGuard	resetLogger { [] { juce::Logger::setCurrentLogger ( nullptr ); } };

	if ( argc < 2 )
	{
		std::printf ( "Usage: hvsc_update <HVSC.zip> [<HVSC_Update_N.7z> ...]\n" );
		return 2;
	}

	const auto	zipFile = fileArg ( argv[ 1 ] );

	if ( ! hvscsource::setRoot ( zipFile ) || ! hvscsource::isZipMode () )
	{
		std::printf ( "Not a zip collection: %s\n", zipFile.getFullPathName ().toRawUTF8 () );
		return 1;
	}

	const auto	installed = installedRelease ();
	if ( installed <= 0 )
	{
		std::printf ( "No release number in DOCUMENTS/HVSC.txt\n" );
		return 1;
	}

	std::printf ( "Release %d\n", installed );

	if ( argc == 2 )
		return 0;

	auto*		zip = hvscsource::archive ();
	const auto	prefix = hvscsource::archivePrefix ();

	// Leftovers of an interrupted run
	zip->discardPendingChanges ();
	zip->removeFolder ( prefix + "update" );

	ZipTree	tree ( *zip, prefix );

	for ( auto i = 2; i < argc; ++i )
	{
		const auto	version = installed + i - 1;
		const auto	archive = fileArg ( argv[ i ] );

		juce::MemoryBlock	data;
		if ( ! archive.loadFileAsData ( data ) || data.isEmpty () )
		{
			std::printf ( "Can't read %s\n", archive.getFullPathName ().toRawUTF8 () );
			zip->discardPendingChanges ();
			return 1;
		}

		std::atomic<float>	progress = 0.0f;
		std::atomic<int>	files = 0, maxFiles = 0;

		std::printf ( "Update %d: extracting %s\n", version, archive.getFileName ().toRawUTF8 () );

		if ( Unarchiver::extractArchiveInto ( *zip, prefix, data, progress, files, maxFiles, hvscLimits ) < 0 )
		{
			std::printf ( "Update %d: extraction failed\n", version );
			zip->discardPendingChanges ();
			return 1;
		}

		// The chain runs in release order, the archive has to be that release
		if ( ! tree.existsAsFile ( "update/Update" + juce::String ( version ) + ".hvs" ) )
		{
			std::printf ( "Update %d: %s is not the update archive for release %d\n", version, archive.getFileName ().toRawUTF8 (), version );
			zip->discardPendingChanges ();
			return 1;
		}

		std::printf ( "Update %d: applying script\n", version );

		const auto	errors = HVSCUpdater ().update ( tree, version, progress, files, maxFiles );
		if ( errors != 0 )
		{
			std::printf ( "Update %d: %d script errors\n", version, errors );
			zip->discardPendingChanges ();
			return 1;
		}
	}

	std::printf ( "Writing %s\n", zipFile.getFullPathName ().toRawUTF8 () );

	if ( ! tree.finish () )
	{
		std::printf ( "Commit failed, the collection is unchanged\n" );
		return 1;
	}

	// Re-open to report what the collection carries now
	if ( ! hvscsource::setRoot ( zipFile ) )
	{
		std::printf ( "Can't re-open %s\n", zipFile.getFullPathName ().toRawUTF8 () );
		return 1;
	}

	std::printf ( "Release %d\n", installedRelease () );
	return 0;
}
//-----------------------------------------------------------------------------
