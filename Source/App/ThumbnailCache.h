#pragma once

#include <JuceHeader.h>

#include <chrono>

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Helpers/MipMap.h"
#include "ultra-shared/Video/VIC2_Render.h"

//-----------------------------------------------------------------------------

class ThumbnailCache final
{
public:
	ThumbnailCache ();
	~ThumbnailCache ();

	// The mip-map sharpening and saturation lift tuned for the VIC2 renders; anything
	// composed from them should use the same
	static constexpr MipMap::Enhance	vic2Enhance { 1.0f, 1.0f };

	// this
	void reset ();
	void setCacheLimit ( const int maxEntries );
	void refreshDefaultImage ();

	// Use the returned reference immediately, never store it. It stays valid only while
	// nothing evicts or reassigns that entry
	[[ nodiscard ]] MipMap& getThumbnail ( std::string_view tunename, const bool isNTSC, std::function<void()> callback = nullptr );
	void removeCacheEntry ( const std::string& tunename );

	[[ nodiscard ]] juce::Image& getDefaultScreen () { return defaultScreen; }
	[[ nodiscard ]] int getCacheSize () const;
	[[ nodiscard ]] bool isDefaultImage ( const MipMap& img ) const { return std::addressof ( img ) == std::addressof ( defaultImage ); }

	// Bumped by refreshDefaultImage, so consumers can drop baked copies
	[[ nodiscard ]] int getDefaultImageVersion () const { return defaultImageVersion; }

private:
	void clearCache ();
	void removeStaleEntries ();
	[[ nodiscard ]] bool hasCacheEntry ( const std::string& tunename );

	[[ nodiscard ]] MipMap renderThumbnail ( VIC2_Render& vic2, const std::string& artName, const bool isNTSC );
	[[ nodiscard ]] static juce::Image postProcess ( juce::Image img, const int reduceX, const int reduceY );
	[[ nodiscard ]] static juce::Image createImage ( VIC2_Render& vic2, const std::string& artName );

	VIC2_Render::settings	vic2Set { .standard = VIC2_Render::settings::PAL, .contrast = 110.0f };
	VIC2_Render				vic2Thumb { false };
	VIC2_Render				vic2Job { false };	// Job renderer, lock-free: the pool is one thread wide

	juce::CriticalSection	cacheCs;

	struct CacheEntry
	{
		MipMap	image;
		std::chrono::steady_clock::time_point	lastAccess;
	};
	std::unordered_map<std::string, CacheEntry, lime::str::TransparentHash, std::equal_to<>>	cache;

	// One queued render job per tune, every requester's callback joins the
	// entry and fires on completion; guarded by cacheCs
	std::unordered_map<std::string, std::vector<std::function<void ()>>>	pending;

	int	cacheMaxEntries = 200;

	MipMap		defaultImage;
	juce::Image	defaultScreen;
	int			defaultImageVersion = 0;

	// Last member: destroyed first, so a still-running render job is joined
	// while the lock, cache and renderer it uses are all alive
	juce::ThreadPool	threadPool { 1, juce::Thread::osDefaultStackSize, juce::Thread::Priority::low };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( ThumbnailCache )
};
//-----------------------------------------------------------------------------
