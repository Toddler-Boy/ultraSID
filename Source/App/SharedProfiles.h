#pragma once

#include <juce_core/juce_core.h>
#include <lime_Logger/lime_Logger.h>
#include <memory>

#include "libSidplayEZ/src/EZ/shared-config.h"

#include "ultra-shared/Config/DataSource.h"

//-----------------------------------------------------------------------------

class SharedProfiles
{
public:
	// Shared player configuration: sidid signatures, chip/audio profiles and
	// tune overrides, parsed once and handed to every player. To change parts
	// of it, copy the current config, reload the changed part on the copy and
	// publish it here, players keep whatever config they were given
	[[ nodiscard ]] std::shared_ptr<const libsidplayEZ::SharedPlayerConfig> getPlayerConfig () const
	{
		const juce::ScopedLock	sl ( lock );
		return playerConfig;
	}
	//-----------------------------------------------------------------------------

	void setPlayerConfig ( std::shared_ptr<const libsidplayEZ::SharedPlayerConfig> config )
	{
		const juce::ScopedLock	sl ( lock );
		playerConfig = std::move ( config );
	}
	//-----------------------------------------------------------------------------

	// ROMs
	void loadRoms ()
	{
		auto load = [] ( const juce::String& name, const size_t expectedSize )
		{
			auto	mb = datasource::loadData ( "Roms/" + name );

			// The engine copies fixed-size ROM blocks, a short file must count as missing
			if ( mb.getSize () != 0 && mb.getSize () != expectedSize )
			{
				Z_ERR ( name << " has wrong size: " << juce::String ( mb.getSize () ) << " bytes" );
				mb.reset ();
			}

			return mb;
		};

		auto	kernal = load ( "kernal.bin", 8192 );
		auto	basic = load ( "basic.bin", 8192 );
		auto	character = load ( "chargen.bin", 4096 );

		const juce::ScopedLock	sl ( lock );
		romKernal = std::move ( kernal );
		romBasic = std::move ( basic );
		romCharacter = std::move ( character );
	}
	//-----------------------------------------------------------------------------

	[[ nodiscard ]] std::tuple<const void*, const void*, const void*> getRoms () const
	{
		const juce::ScopedLock	sl ( lock );

		return { romKernal.getData (), romBasic.getData (), romCharacter.getData () };
	}
	//-----------------------------------------------------------------------------

private:
	juce::CriticalSection	lock;

	std::shared_ptr<const libsidplayEZ::SharedPlayerConfig>	playerConfig;

	juce::MemoryBlock	romKernal;
	juce::MemoryBlock	romBasic;
	juce::MemoryBlock	romCharacter;
};
//-----------------------------------------------------------------------------
