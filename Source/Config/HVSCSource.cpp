#include <JuceHeader.h>

#include "HVSCSource.h"

#include "ultra-shared/Config/ZipFolder.h"

//-----------------------------------------------------------------------------

namespace
{
	struct State
	{
		juce::File		root;
		ZipFolder		zip;
		juce::String	prefix;		// "C64Music/" when the archive wraps the folder itself
		bool			zipMode = false;
	};

	State& state ()
	{
		static State	s;
		return s;
	}

	[[ nodiscard ]] juce::String stripLeadingSlash ( const juce::String& path )
	{
		return path.startsWithChar ( '/' ) ? path.substring ( 1 ) : path;
	}

	// Same probe for the live ZipFolder and a transient PakFile
	[[ nodiscard ]] bool zipPathsValid ( const auto& archive, const juce::String& prefix, const juce::StringArray& arr )
	{
		for ( const auto& f : arr )
		{
			if ( f.endsWithChar ( '/' ) )
			{
				if ( ! archive.folderExists ( prefix + f ) )
					return false;
			}
			else if ( ! archive.exists ( prefix + f ) )
			{
				return false;
			}
		}

		return true;
	}
}
//-----------------------------------------------------------------------------

bool hvscsource::isZipArchive ( const juce::File& file )
{
	return file.existsAsFile () && PakFile::hasZipTail ( file );
}
//-----------------------------------------------------------------------------

bool hvscsource::setRoot ( const juce::File& rootOrZip )
{
	auto&	s = state ();

	s.root = rootOrZip;
	s.zipMode = false;
	s.prefix.clear ();
	s.zip = ZipFolder ();

	if ( ! isZipArchive ( rootOrZip ) )
		return rootOrZip.isDirectory ();

	if ( ! s.zip.open ( rootOrZip ) )
		return false;

	s.zipMode = true;

	if ( ! s.zip.exists ( "DOCUMENTS/HVSC.txt" ) && s.zip.exists ( "C64Music/DOCUMENTS/HVSC.txt" ) )
		s.prefix = "C64Music/";

	return true;
}
//-----------------------------------------------------------------------------

bool hvscsource::isZipMode ()
{
	return state ().zipMode;
}
//-----------------------------------------------------------------------------

ZipFolder* hvscsource::archive ()
{
	auto&	s = state ();

	return s.zipMode ? &s.zip : nullptr;
}
//-----------------------------------------------------------------------------

juce::String hvscsource::archivePrefix ()
{
	return state ().prefix;
}
//-----------------------------------------------------------------------------

bool hvscsource::exists ( const juce::String& path )
{
	auto&	s = state ();

	if ( s.zipMode )
		return s.zip.exists ( s.prefix + stripLeadingSlash ( path ) );

	return s.root.getChildFile ( stripLeadingSlash ( path ) ).existsAsFile ();
}
//-----------------------------------------------------------------------------

bool hvscsource::folderExists ( const juce::String& path )
{
	auto&	s = state ();

	if ( s.zipMode )
		return s.zip.folderExists ( s.prefix + stripLeadingSlash ( path ) );

	return s.root.getChildFile ( stripLeadingSlash ( path ) ).isDirectory ();
}
//-----------------------------------------------------------------------------

juce::MemoryBlock hvscsource::loadData ( const juce::String& path )
{
	auto&	s = state ();

	if ( s.zipMode )
		return s.zip.load ( s.prefix + stripLeadingSlash ( path ) );

	juce::MemoryBlock	mb;

	if ( const auto f = s.root.getChildFile ( stripLeadingSlash ( path ) ); f.existsAsFile () )
		f.loadFileAsData ( mb );

	return mb;
}
//-----------------------------------------------------------------------------

juce::String hvscsource::loadText ( const juce::String& path )
{
	const auto	mb = loadData ( path );

	return juce::String::createStringFromData ( mb.getData (), int ( mb.getSize () ) );
}
//-----------------------------------------------------------------------------

std::unique_ptr<juce::InputStream> hvscsource::createStream ( const juce::String& path )
{
	auto&	s = state ();

	if ( s.zipMode )
		return s.zip.createStream ( s.prefix + stripLeadingSlash ( path ) );

	auto	in = std::make_unique<juce::FileInputStream> ( s.root.getChildFile ( stripLeadingSlash ( path ) ) );

	return in->openedOk () ? std::move ( in ) : nullptr;
}
//-----------------------------------------------------------------------------

bool hvscsource::allPathsValid ( const juce::File& rootOrZip, const juce::StringArray& arr )
{
	if ( ! isZipArchive ( rootOrZip ) )
	{
		if ( ! rootOrZip.isDirectory () )
			return false;

		for ( const auto& f : arr )
		{
			if ( f.endsWithChar ( '/' ) )
			{
				if ( ! rootOrZip.getChildFile ( f ).isDirectory () )
					return false;
			}
			else if ( ! rootOrZip.getChildFile ( f ).existsAsFile () )
			{
				return false;
			}
		}

		return true;
	}

	auto&	s = state ();

	if ( s.zipMode && rootOrZip == s.root )
		return zipPathsValid ( s.zip, s.prefix, arr );

	PakFile	probe;

	if ( ! probe.open ( rootOrZip ) )
		return false;

	const auto	prefix = probe.exists ( "DOCUMENTS/HVSC.txt" ) ? juce::String () : juce::String ( "C64Music/" );

	return zipPathsValid ( probe, prefix, arr );
}
//-----------------------------------------------------------------------------

void hvscsource::loadBytes ( const char* fileName, std::vector<uint8_t>& bufferRef )
{
	const auto	mb = loadData ( fileName );
	const auto	data = static_cast<const uint8_t*> ( mb.getData () );

	bufferRef.assign ( data, data + mb.getSize () );
}
//-----------------------------------------------------------------------------
