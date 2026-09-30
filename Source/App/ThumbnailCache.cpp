#include "ThumbnailCache.h"

#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/ImageUtils.h"

#include "UI/Pages/CRT/GUI_CRT.h"

#include "ScreenshotLookup.h"


//-----------------------------------------------------------------------------

ThumbnailCache::ThumbnailCache ()
	: juce::Thread ( "Thumbnails" )
{
	refreshDefaultImage ();

	startThread ( juce::Thread::Priority::low );
}
//-----------------------------------------------------------------------------

// The dummy image is a boot-screen layout, so it matches the CRT page
void ThumbnailCache::refreshDefaultImage ()
{
	// Synchronous renders may have left other settings and content behind
	vic2Thumb.setSettings ( vic2Set );
	vic2Thumb.invalidate ();

	GUI_CRT::playerLayout	layout;

	if ( const auto name = GUI_CRT::pickLayoutFile ( true ); name.isNotEmpty () && GUI_CRT::loadLayoutFile ( name, layout ) )
	{
		vic2Thumb.screenCol = layout.screenCol;
		vic2Thumb.borderCol = layout.borderCol;
		vic2Thumb.controlByte = layout.controlByte;
		vic2Thumb.setCustomCharset ( layout.customFont.empty () ? nullptr : layout.customFont.data () );

		std::copy_n ( layout.screen, std::size ( layout.screen ), vic2Thumb.screenBuffer );
		std::copy_n ( layout.color, std::size ( layout.color ), vic2Thumb.colorBuffer );
	}
	else
	{
		vic2Thumb.screenCol = vic2::black;
		vic2Thumb.borderCol = vic2::black;

		std::fill_n ( vic2Thumb.screenBuffer, VIC2_Render::textColumns * VIC2_Render::textRows, uint8_t ( 32 ) );
		std::fill_n ( vic2Thumb.colorBuffer, VIC2_Render::textColumns * VIC2_Render::textRows, uint8_t ( vic2::light_grey ) );
	}

	vic2Thumb.renderScreen ();

	// The font bits die with this scope
	vic2Thumb.setCustomCharset ( nullptr );

	defaultScreen = vic2Thumb.getThumbnail ().createCopy ();
	defaultImage.setImage ( postProcess ( defaultScreen.createCopy (), 0, 8 ), vic2Enhance );

	++defaultImageVersion;
}
//-----------------------------------------------------------------------------

ThumbnailCache::~ThumbnailCache ()
{
	stopThread ( -1 );
}
//-----------------------------------------------------------------------------

MipMap& ThumbnailCache::getThumbnail ( const std::string_view tunenameView, const bool isNTSC, std::function<void ()> callback )
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	//
	// Get item from cache
	//
	if ( auto it = cache.find ( tunenameView ); it != cache.end () )
	{
		it->second.lastAccess = std::chrono::steady_clock::now ();
		return it->second.image;
	}

	const std::string	tunename ( tunenameView );

	//
	// Find item in artwork list
	//
	const juce::SharedResourcePointer<ScreenshotLookup>	scrSht;

	const auto	artName = scrSht->getDefaultScreenshot ( tunename );
	if ( artName.empty () )
		return defaultImage;

	return getOrRender ( tunename, artName, isNTSC, false, std::move ( callback ) );
}
//-----------------------------------------------------------------------------

MipMap& ThumbnailCache::getArtThumbnail ( const std::string& artName, const bool isNTSC, std::function<void ()> callback )
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	// Art names never start with a location marker, so they share the map
	// with the tune keys
	if ( auto it = cache.find ( artName ); it != cache.end () )
	{
		it->second.lastAccess = std::chrono::steady_clock::now ();
		return it->second.image;
	}

	return getOrRender ( artName, artName, isNTSC, true, std::move ( callback ) );
}
//-----------------------------------------------------------------------------

MipMap& ThumbnailCache::getOrRender ( const std::string& key, const std::string& artName, const bool isNTSC, const bool analyze, std::function<void ()> callback )
{
	// No callback: render the thumbnail synchronously and return it
	if ( ! callback )
	{
		cache[ key ] = renderThumbnail ( vic2Thumb, artName, isNTSC, analyze );
		return cache[ key ].image;
	}

	// Asked again: the waiting render moves to the back, the callback joins the waiters
	if ( auto it = std::ranges::find ( pending, key, &Request::key ); it != pending.end () )
	{
		it->callbacks.emplace_back ( std::move ( callback ) );
		std::rotate ( it, it + 1, pending.end () );
		return defaultImage;
	}

	pending.push_back ( { key, artName, isNTSC, analyze, {} } );
	pending.back ().callbacks.emplace_back ( std::move ( callback ) );

	if ( pending.size () > maxPendingRenders )
		pending.erase ( pending.begin () );

	notify ();

	return defaultImage;
}
//-----------------------------------------------------------------------------

void ThumbnailCache::run ()
{
	while ( ! threadShouldExit () )
	{
		Request	request;

		{
			const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

			if ( ! pending.empty () )
			{
				request = std::move ( pending.back () );
				pending.pop_back ();
			}
		}

		if ( request.key.empty () )
		{
			wait ( -1 );
			continue;
		}

		// The synchronous path may have cached this key already
		auto	entry = hasCacheEntry ( request.key ) ? CacheEntry {} : renderThumbnail ( vic2Job, request.artName, request.isNTSC, request.analyze );

		{
			const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

			// Insert only, never reassign: getThumbnail hands out references into the entry
			if ( ! cache.contains ( request.key ) )
			{
				removeStaleEntries ();
				cache[ request.key ] = std::move ( entry );
			}
		}

		for ( auto& cb : request.callbacks )
			juce::MessageManager::callAsync ( std::move ( cb ) );
	}
}
//-----------------------------------------------------------------------------

ThumbnailCache::CacheEntry ThumbnailCache::renderThumbnail ( VIC2_Render& vic2, const std::string& artName, const bool isNTSC, const bool analyze )
{
	const auto	hints = imageutils::hintFromFilename ( artName );

	auto	set = vic2Set;
	set.standard = isNTSC || hints.forceNTSC ? VIC2_Render::settings::NTSC : VIC2_Render::settings::PAL;
	set.firstLuma = hints.firstLuma;
	vic2.setSettings ( set );

	std::optional<uint16_t>	pictureFlags;
	auto	img = createImage ( vic2, artName, analyze ? &pictureFlags : nullptr );

	return { MipMap ( img.convertedToFormat ( juce::Image::PixelFormat::RGB ).rescaled ( img.getWidth () / 2, img.getHeight () / 2 ), vic2Enhance ),
			 artName,
			 std::chrono::steady_clock::now (),
			 vic2.getNumFields () > 1,
			 vic2.getScrollRangeX () > 0 || vic2.getScrollRangeY () > 0,
			 pictureFlags };
}
//-----------------------------------------------------------------------------

bool ThumbnailCache::isInterlaced ( const std::string& key ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	const auto	it = cache.find ( key );

	return it != cache.end () && it->second.interlaced;
}
//-----------------------------------------------------------------------------

bool ThumbnailCache::isScrolling ( const std::string& key ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	const auto	it = cache.find ( key );

	return it != cache.end () && it->second.scrolling;
}
//-----------------------------------------------------------------------------

std::optional<uint16_t> ThumbnailCache::getPictureFlags ( const std::string& key ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	const auto	it = cache.find ( key );
	if ( it == cache.end () )
		return std::nullopt;

	return it->second.pictureFlags;
}
//-----------------------------------------------------------------------------

bool ThumbnailCache::hasCacheEntry ( const std::string& tunename )
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	return cache.contains ( tunename );
}
//-----------------------------------------------------------------------------

void ThumbnailCache::removeCacheEntry ( const std::string& tunename )
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	cache.erase ( tunename );
}
//-----------------------------------------------------------------------------

void ThumbnailCache::removeArtEntries ( const std::string& artName )
{
	const juce::SharedResourcePointer<ScreenshotLookup>	lookup;

	// A curation can move the tune's default pick to another of its screenshots
	auto	affected = lookup->getSiblings ( artName );
	affected.emplace_back ( artName );

	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	std::erase_if ( cache, [ &affected ] ( const auto& entry ) { return std::ranges::contains ( affected, entry.second.artName ); } );
}
//-----------------------------------------------------------------------------

int ThumbnailCache::getCacheSize () const
{
	return std::accumulate ( cache.begin (), cache.end (), 0, [] ( const auto acc, const auto& entry )	{
		return acc + entry.second.image.getNumBytesOfData ();
	} );
}
//-----------------------------------------------------------------------------

void ThumbnailCache::clearCache ()
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	cache.clear ();
}
//-----------------------------------------------------------------------------

void ThumbnailCache::removeStaleEntries ()
{
	const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

	if ( cache.size () <= cacheMaxEntries )
		return;

	// Remove outdated entries
	auto	entriesToRemove = cache.size () - cacheMaxEntries;
	while ( entriesToRemove-- > 0 )
	{
		auto	oldestIt = cache.begin ();

		for ( auto it = oldestIt; it != cache.end (); ++it )
			if ( it->second.lastAccess < oldestIt->second.lastAccess )
				oldestIt = it;

		cache.erase ( oldestIt );
	}

//	Z_INFO ( "Cache entries: " << cache.size () << " Cache size : " << textutils::getHumanNumber ( getCacheSize () ) );
}
//-----------------------------------------------------------------------------

void ThumbnailCache::reset ()
{
	clearCache ();
}
//-----------------------------------------------------------------------------

void ThumbnailCache::setCacheLimit ( const int maxEntries )
{
	cacheMaxEntries = maxEntries;

	removeStaleEntries ();
}
//-----------------------------------------------------------------------------

juce::Image ThumbnailCache::createImage ( VIC2_Render& vic2, const std::string& artName, std::optional<uint16_t>* pictureFlags )
{
	const juce::SharedResourcePointer<ScreenshotLookup>	scrSht;

	const auto	mb = scrSht->loadData ( artName );

	if ( vic2.loadImage ( artName.c_str (), mb.getData (), mb.getSize () ) && pictureFlags )
		*pictureFlags = vic2.analyze ();

	vic2.renderCRT ();

	const auto	div = vic2.wasBorderFilled () * 1 + 1;

	return postProcess ( vic2.getThumbnail (), VIC2_Render::unscaledBorderSizeX / div, VIC2_Render::unscaledBorderSizeY / div );
}
//-----------------------------------------------------------------------------

juce::Image ThumbnailCache::postProcess ( juce::Image img, const int reduceX, const int reduceY )
{
	img = img.getClippedImage ( img.getBounds ().reduced ( reduceX, reduceY ) );

	constexpr auto	thumbWidth = int ( 320 * VIC2::truePalX + 0.49f );
	constexpr auto	thumbHeight = 200;

	return img.rescaled ( thumbWidth, thumbHeight );
}
//-----------------------------------------------------------------------------
