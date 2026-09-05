#include "ThumbnailCache.h"

#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/ImageUtils.h"

#include "UI/Pages/CRT/GUI_CRT.h"

#include "ScreenshotLookup.h"


//-----------------------------------------------------------------------------

ThumbnailCache::ThumbnailCache ()
{
	refreshDefaultImage ();
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
	// Drop queued jobs and wait for the running one, it uses the members below
	threadPool.removeAllJobs ( false, -1 );
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

	// No callback: render the thumbnail synchronously and return it
	if ( ! callback )
	{
		cache[ tunename ] = { renderThumbnail ( vic2Thumb, artName, isNTSC ), std::chrono::steady_clock::now () };
		return cache[ tunename ].image;
	}

	// A render for this tune is already queued, the callback joins the waiters
	if ( auto it = pending.find ( tunename ); it != pending.end () )
	{
		it->second.emplace_back ( std::move ( callback ) );
		return defaultImage;
	}

	pending[ tunename ].emplace_back ( std::move ( callback ) );

	//
	// Add job to thread pool
	//
	threadPool.addJob ( [ this, tunename, artName, isNTSC ]
	{
		// The synchronous path may have cached this tune already
		auto	image = hasCacheEntry ( tunename ) ? MipMap {} : renderThumbnail ( vic2Job, artName, isNTSC );

		std::vector<std::function<void ()>>	callbacks;

		{
			const juce::CriticalSection::ScopedLockType	csLock ( cacheCs );

			// Insert only, never reassign: getThumbnail hands out references into the entry
			if ( ! cache.contains ( tunename ) )
			{
				removeStaleEntries ();
				cache[ tunename ] = { std::move ( image ), std::chrono::steady_clock::now () };
			}

			if ( auto it = pending.find ( tunename ); it != pending.end () )
			{
				callbacks = std::move ( it->second );
				pending.erase ( it );
			}
		}

		for ( auto& cb : callbacks )
			juce::MessageManager::callAsync ( std::move ( cb ) );
	} );

	return defaultImage;
}
//-----------------------------------------------------------------------------

MipMap ThumbnailCache::renderThumbnail ( VIC2_Render& vic2, const std::string& artName, const bool isNTSC )
{
	const auto	hints = imageutils::hintFromFilename ( artName );

	auto	set = vic2Set;
	set.standard = isNTSC ? VIC2_Render::settings::NTSC : VIC2_Render::settings::PAL;
	set.firstLuma = hints.firstLuma;
	vic2.setSettings ( set );

	auto	img = createImage ( vic2, artName );

	return MipMap ( img.convertedToFormat ( juce::Image::PixelFormat::RGB ).rescaled ( img.getWidth () / 2, img.getHeight () / 2 ), vic2Enhance );
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

juce::Image ThumbnailCache::createImage ( VIC2_Render& vic2, const std::string& artName )
{
	const auto	mb = datasource::loadData ( "Screenshots/" + artName );

	vic2.loadImage ( artName.c_str (), mb.getData (), mb.getSize () );
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
