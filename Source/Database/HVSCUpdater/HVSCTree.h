#pragma once

#include <JuceHeader.h>

#include <string>
#include <unordered_set>

class ZipFolder;

//-----------------------------------------------------------------------------

// The collection surface HVSCUpdater mutates, one implementation per storage
// mode. Paths are collection-relative with forward slashes and no leading
// '/'; directories carry a trailing '/' where the update script writes one

class HVSCTree
{
public:
	virtual ~HVSCTree () = default;

	[[ nodiscard ]] virtual bool exists ( const juce::String& rel ) const = 0;
	[[ nodiscard ]] virtual bool existsAsFile ( const juce::String& rel ) const = 0;

	// Every path in the collection with a leading '/', directories with a
	// trailing '/' as well: the updater's case-correction index
	[[ nodiscard ]] virtual juce::StringArray listAllForCaseIndex () const = 0;

	// Names of the immediate files under the folder
	[[ nodiscard ]] virtual juce::StringArray listChildFiles ( const juce::String& folder ) const = 0;

	[[ nodiscard ]] virtual juce::MemoryBlock read ( const juce::String& rel ) const = 0;
	virtual bool write ( const juce::String& rel, const juce::MemoryBlock& data ) = 0;
	virtual bool remove ( const juce::String& rel ) = 0;
	virtual bool removeTree ( const juce::String& rel ) = 0;
	virtual bool mkdir ( const juce::String& rel ) = 0;
	virtual bool move ( const juce::String& from, const juce::String& to ) = 0;

	// The point everything becomes real for a staged store (the zip commit);
	// a plain folder was mutated in place all along
	virtual bool finish () = 0;
};
//-----------------------------------------------------------------------------

// The loose C64Music folder, mutated in place

class FolderTree final : public HVSCTree
{
public:
	explicit FolderTree ( const juce::File& _root ) : root ( _root ) {}

	[[ nodiscard ]] bool exists ( const juce::String& rel ) const override;
	[[ nodiscard ]] bool existsAsFile ( const juce::String& rel ) const override;
	[[ nodiscard ]] juce::StringArray listAllForCaseIndex () const override;
	[[ nodiscard ]] juce::StringArray listChildFiles ( const juce::String& folder ) const override;
	[[ nodiscard ]] juce::MemoryBlock read ( const juce::String& rel ) const override;
	bool write ( const juce::String& rel, const juce::MemoryBlock& data ) override;
	bool remove ( const juce::String& rel ) override;
	bool removeTree ( const juce::String& rel ) override;
	bool mkdir ( const juce::String& rel ) override;
	bool move ( const juce::String& from, const juce::String& to ) override;
	bool finish () override	{	return true;	}

private:
	[[ nodiscard ]] juce::File fileFor ( const juce::String& rel ) const;

	juce::File	root;
};
//-----------------------------------------------------------------------------

// The zip-backed collection: mutations stage in the ZipFolder overlay,
// finish() commits once. ghostDirs keeps created or emptied folders alive
// until deleted, matching loose-tree semantics

class ZipTree final : public HVSCTree
{
public:
	ZipTree ( ZipFolder& _zip, const juce::String& _prefix ) : zip ( _zip ), prefix ( _prefix ) {}

	[[ nodiscard ]] bool exists ( const juce::String& rel ) const override;
	[[ nodiscard ]] bool existsAsFile ( const juce::String& rel ) const override;
	[[ nodiscard ]] juce::StringArray listAllForCaseIndex () const override;
	[[ nodiscard ]] juce::StringArray listChildFiles ( const juce::String& folder ) const override;
	[[ nodiscard ]] juce::MemoryBlock read ( const juce::String& rel ) const override;
	bool write ( const juce::String& rel, const juce::MemoryBlock& data ) override;
	bool remove ( const juce::String& rel ) override;
	bool removeTree ( const juce::String& rel ) override;
	bool mkdir ( const juce::String& rel ) override;
	bool move ( const juce::String& from, const juce::String& to ) override;
	bool finish () override;

private:
	// Lowered collection-relative folder path with a trailing '/'
	[[ nodiscard ]] static std::string ghostKey ( const juce::String& folder );

	ZipFolder&		zip;
	juce::String	prefix;

	std::unordered_set<std::string>	ghostDirs;
};
//-----------------------------------------------------------------------------
