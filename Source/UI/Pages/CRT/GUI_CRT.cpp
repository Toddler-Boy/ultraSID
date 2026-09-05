#include <JuceHeader.h>

#include "GUI_CRT.h"

#include "libSidplayEZ/src/stringutils.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/ComponentUtils.h"
#include "ultra-shared/Helpers/ImageUtils.h"
#include "ultra-shared/Helpers/TextUtils.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Database/TuneInfo.h"
#include "Helpers/Messages.h"
#include "UI/Components/FFT/fft-helpers.h"

//-----------------------------------------------------------------------------

namespace
{
	// The frequency strip plays by VIC-II rules: 8 Y-expanded sprites in
	// lockstep sharing one bar pattern, one per history frame; per voice band
	// only X and color change, rewritten in the doubled gap lines. Never more
	// than 8 markers per raster line, one color each
	constexpr auto	stripBands = SID::numVoices;
	constexpr auto	bandPitch = 8;
	constexpr auto	barHeight = 6;
	constexpr auto	barWidth = 2;

	constexpr auto	stripHeight = stripBands * bandPitch;

	// Fade ramps per voice state from "Frequency colors.json", dark to
	// bright, indexed by brightness; empty draws nothing
	std::vector<uint8_t>	fadeRamp[ 3 ];

	void loadFrequencyColors ();

	// colIdx as in GUI_ChipFrequencyLines: on, filtered, muted, muted-but-filtered
	constexpr uint8_t	fadeRampForState[ 4 ] = { 0, 1, 2, 1 };

}
//-----------------------------------------------------------------------------

GUI_CRT::GUI_CRT ()
{
	setName ( "crt" );
	addAndMakeVisible ( overlay );

	loadFrequencyColors ();

	// Open/close settings
	{
		overlay.openSettings.onClick = [ this ]
		{
			showSettings ( overlay.openSettings.getStage () );
		};
	}

	addChildComponent ( settingsPanel );

	settingsPanel.onSettingsChanged = [ this ]	{	updateOverlayCRTSettings ();	};
	settingsPanel.onOverlayChanged = [ this ]	{	overlay.updateOverlay ();		};
	settingsPanel.onZoomChanged = [ this ]		{	overlay.updateZoom ();			};

	settingsPanel.autoSystem = [ this ]			{	return juce::String ( sidInfoStr.clock );	};
	settingsPanel.autoFirstLuma = [ this ]		{	return lastFirstLuma;	};

	addMouseListener ( this, true );
}
//-----------------------------------------------------------------------------

void GUI_CRT::resized ()
{
	const auto	kioskMode = dynamic_cast<juce::DocumentWindow*> ( getTopLevelComponent () )->isKioskMode ();

	crtLayout.setConstant ( "fullscreen", kioskMode ? 1 : 0 );
	crtLayout.setConstant ( "windowed", kioskMode ? 0 : 1 );
	crtLayout.setConstant ( "showSettings", settingsVisible ? 1 : 0 );

	UI::setLayout ( crtLayout, {	"UI/layouts/constants.json",
								"UI/layouts/crt.json" } );
}
//-----------------------------------------------------------------------------

void GUI_CRT::mouseWheelMove ( const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel )
{
	if ( event.eventComponent != &overlay )
		return;

	const auto	delta = wheel.deltaY * ( event.mods.isShiftDown () ? 20.0f : 50.0f );

	if ( event.mods.isCommandDown () )
	{
		auto	overscan = componentutils::findComponent<juce::Slider> ( "tv/overscan/slider", settingsPanel.componentMap () );

		const auto	newOverscan = std::clamp ( float ( overscan->getValue () ) + delta, 0.0f, 100.0f );
		overscan->setValue ( newOverscan, juce::dontSendNotification );
		preferences->set ( "tv/overscan", newOverscan );
	}
	else
	{
		if ( ! preferences->get<bool> ( "overlay/enabled" ) )
			return;

 		auto	zoom = componentutils::findComponent<juce::Slider> ( "overlay/disabler/zoom/slider", settingsPanel.componentMap () );

 		const auto	newZoom = std::clamp ( zoom->getValue () + delta, 0.0, 100.0 );
 		zoom->setValue ( newZoom, juce::dontSendNotification );
 		preferences->set ( "overlay/zoom", int ( newZoom ) );
	}

	updateOverlayCRTSettings ();
	overlay.updateZoom ();
}
//-----------------------------------------------------------------------------

juce::File GUI_CRT::getLastLoadedFile ()
{
	return lastLoadedName.isEmpty () ? juce::File () : datasource::getDevFile ( "Screenshots/" + lastLoadedName );
}
//-----------------------------------------------------------------------------

bool GUI_CRT::loadArtworkImage ()
{
	if ( lastLoadedName.isEmpty () )
		return false;

	const auto	mb = datasource::loadData ( "Screenshots/" + lastLoadedName );

	return vicRender.loadImage ( lastLoadedName.toRawUTF8 (), mb.getData (), mb.getSize () );
}
//-----------------------------------------------------------------------------

void GUI_CRT::setStrings ( const SidTuneInfoEZ& src )
{
	sidInfoStr = src;
	composeDirty = true;
}
//-----------------------------------------------------------------------------

void GUI_CRT::loadGameArtwork ( const juce::String& sidName, const juce::String& index /* ="" */ )
{
	std::tie ( lastLoadedName, tuneArtIndex ) = findArtwork ( sidName, index );

	freqFramesUsed = 0;

	lastFirstLuma = imageutils::hintFromFilename ( lastLoadedName.toStdString () ).firstLuma;

	sidname = sidName.fromLastOccurrenceOf ( "/", false, false );

	loadPlayerLayout ( sidname.isEmpty () );

	if ( loadArtworkImage () )
		renderCRT ();
	else
		renderCRT ( true );
}
//-----------------------------------------------------------------------------

void GUI_CRT::loadGameArtwork ( const int index )
{
	if ( index < 0 || index >= int ( tuneArtwork.size () ) )
	{
		lastLoadedName = {};
		renderCRT ( true );
		return;
	}

	tuneArtIndex = index;
	lastLoadedName = tuneArtwork[ size_t ( tuneArtIndex ) ];

	lastFirstLuma = imageutils::hintFromFilename ( lastLoadedName.toStdString () ).firstLuma;

	if ( loadArtworkImage () )
		renderCRT ();
	else
		renderCRT ( true );
}
//-----------------------------------------------------------------------------

void GUI_CRT::showSettings ( const bool visible )
{
	// Cameras hot-plug (and OBS virtual ones come and go), refresh per open
	if ( visible )
		settingsPanel.refreshWebcamDevices ();

	settingsPanel.setVisible ( visible );
	settingsVisible = visible;
	resized ();
}
//-----------------------------------------------------------------------------

void GUI_CRT::setBackgroundColour ( const juce::Colour& bckCol )
{
	overlay.setBackgroundColor ( bckCol );
}
//-----------------------------------------------------------------------------

void GUI_CRT::paintIntoSnapshot ( juce::Image& snapshot, juce::Component& top )
{
	if ( ! isShowing () )
		return;

	const auto	frame = overlay.grabFrame ();

	if ( ! frame.isValid () )
		return;

	juce::Graphics	g ( snapshot );
	g.drawImage ( frame, top.getLocalArea ( &overlay, overlay.getLocalBounds () ).toFloat () );
}
//-----------------------------------------------------------------------------

void GUI_CRT::timerUpdate ( const float secondsPassed, const uint16_t cpuCycles )
{
	if ( ! isShowing () )
		return;

	// Update OpenGL iFrame & iTime
	overlay.setFrameAndTime ( 0, float ( juce::Time::highResolutionTicksToSeconds ( juce::Time::getHighResolutionTicks () ) ) );

	// This gets called once per V-BLANK (so may be higher than refresh rate of the C64)
	constexpr auto	frameMS = 1.0f / 60.0f - 0.001f;

	// Handle updates (skip if it happened faster than 65 Hz)
	timePassed += secondsPassed;
	if ( timePassed < frameMS )
		return;

	timePassed = 0.0f;

	const auto	haveRaster = cpuCycles >= 50 && cpuCycles <= 10'000;

	if ( lastWasGenerated )
	{
		// Layout screens re-render from their text buffers every frame,
		// no backup/restore involved; the texture only uploads when
		// something actually touched the buffer
		++washPhase;
		const auto	render = updatePlayerScreen ();

		auto	dst = vicRender.getIndexPixels ();
		auto	changed = render.changed;

		changed |= drawLayoutRasterbars ( dst, render.full );

		// The raster-time display conflicts with a layout's own bars
		if ( haveRaster && layout.rasterbars.empty () )
		{
			drawRasterBars ( dst, cpuCycles );
			changed = true;
		}
		else if ( prevCpuBarLines > 0 )
		{
			eraseCpuRasterBars ( dst );
			changed = true;
		}

		changed |= drawProgressBar ( dst );

		if ( freqFramesUsed > 0 && stripW > 0 && ( freqDataChanged || render.full ) )
		{
			drawFrequencyStrip ( dst );
			freqDataChanged = false;
			changed = true;
		}

		if ( changed )
			overlay.triggerIndexTextureUpdate ();

		return;
	}

	if ( ! haveRaster )
		return;

	vicRender.restoreIndexBuffer ();

	drawRasterBars ( vicRender.getIndexPixels (), cpuCycles );

	// The bars sit in the buffer unseen by renderScreen: without this, the
	// cursor blink's render would bake them into the next backup
	vicRender.invalidate ();

	overlay.triggerIndexTextureUpdate ();
}
//-----------------------------------------------------------------------------

namespace
{
	// Fill the border pixels of one line, clipped to the picture: the $d020
	// write of the fiction
	void fillBorderLine ( uint8_t* dst, const int y, const uint8_t color )
	{
		if ( y < 0 || y >= VIC2_Render::outerUnscaledHeight )
			return;

		const auto	d = dst + y * VIC2_Render::outerUnscaledWidth;

		if ( y < VIC2_Render::unscaledBorderSizeY || y >= VIC2_Render::unscaledBorderSizeY + VIC2_Render::innerUnscaledHeight )
		{
			std::fill_n ( d, VIC2_Render::outerUnscaledWidth, color );
		}
		else
		{
			std::fill_n ( d, VIC2_Render::unscaledBorderSizeX, color );
			std::fill_n ( d + VIC2_Render::unscaledBorderSizeX + VIC2_Render::innerUnscaledWidth, VIC2_Render::unscaledBorderSizeX, color );
		}
	}
}
//-----------------------------------------------------------------------------

bool GUI_CRT::drawLayoutRasterbars ( uint8_t* dst, const bool force )
{
	if ( layout.rasterbars.empty () && prevBarSpans.empty () )
		return false;

	// Depth from the weave: bars on the way down pass in front of bars on
	// the way up, exactly like a coded copper routine sorting its table
	struct entry
	{
		float	angle = 0.0f;
		int		top = 0;
		const playerLayout::rasterbar*	bar = nullptr;
	};

	std::vector<entry>	order;

	for ( const auto& bar : layout.rasterbars )
	{
		const auto	angle = juce::MathConstants<float>::twoPi * ( float ( washPhase ) / float ( bar.period ) + bar.phase );
		const auto	top = VIC2_Render::unscaledBorderSizeY + bar.y
						+ int ( std::round ( std::sin ( angle ) * float ( bar.amplitude ) ) )
						- int ( bar.colors.size () ) / 2;

		order.push_back ( { angle, top, &bar } );
	}

	// Overlaps make the bars interdependent: any movement redraws them all,
	// none means the buffer is still right
	auto	moved = force || order.size () != prevBarSpans.size ();
	for ( size_t i = 0; ! moved && i < order.size (); ++i )
		moved = order[ i ].top != prevBarSpans[ i ].first;

	if ( ! moved )
		return false;

	// Erase last frame's spans, then paint back to front
	for ( const auto& [ top, height ] : prevBarSpans )
		for ( auto y = top; y < top + height; ++y )
			fillBorderLine ( dst, y, vicRender.borderCol );

	prevBarSpans.clear ();
	for ( const auto& e : order )
		prevBarSpans.emplace_back ( e.top, int ( e.bar->colors.size () ) );

	std::ranges::sort ( order, {}, [] ( const entry& e ) { return std::cos ( e.angle ); } );

	for ( const auto& e : order )
		for ( auto y = e.top; const auto color : e.bar->colors )
			fillBorderLine ( dst, y++, color );

	return true;
}
//-----------------------------------------------------------------------------

void GUI_CRT::eraseCpuRasterBars ( uint8_t* dst )
{
	for ( auto i = 0; i < prevCpuBarLines; ++i )
		fillBorderLine ( dst, ( 100 + i ) % VIC2_Render::outerUnscaledHeight, vicRender.borderCol );

	prevCpuBarLines = 0;
}
//-----------------------------------------------------------------------------

void GUI_CRT::drawRasterBars ( uint8_t* dst, const uint16_t cpuCycles )
{
	eraseCpuRasterBars ( dst );

	auto fillLine = [ &dst ] ( int y, int width, const uint8_t color )
	{
		width = std::clamp ( width, 0, VIC2_Render::outerUnscaledWidth );
		if ( width <= 0 )
			return;

		y = y % VIC2_Render::outerUnscaledHeight;
		y = y + ( ( y >> 31 ) & VIC2_Render::outerUnscaledHeight );

		const auto	d = dst + y * VIC2_Render::outerUnscaledWidth;

		// In border?
		if ( y < VIC2_Render::unscaledBorderSizeY || y > ( VIC2_Render::unscaledBorderSizeY + VIC2_Render::innerUnscaledHeight ) )
		{
			std::fill_n ( d, width, color );
		}
		else
		{
			std::fill_n ( d, std::min ( width, VIC2_Render::unscaledBorderSizeX ), color );

			width -= VIC2_Render::unscaledBorderSizeX + VIC2_Render::innerUnscaledWidth;
			if ( width > 0 )
				std::fill_n ( d + VIC2_Render::unscaledBorderSizeX + VIC2_Render::innerUnscaledWidth, width, color );
		}
	};

	// Convert cycles to rasterlines
	const auto	crtSet = overlay.getSettings ();
	const auto	cyclesPerLine = crtSet.isNTSC ? 65 : 63;
	const auto	lineWidth = cyclesPerLine * 8;
	const auto	fullLines = cpuCycles / cyclesPerLine;
	const auto	remainder = ( cpuCycles - ( fullLines * cyclesPerLine ) ) * 8;

	// Draw raster lines
	for ( auto i = 0; i < int ( fullLines ); ++i )
		fillLine ( 100 + i, lineWidth, 13 );
	fillLine ( 100 + fullLines, remainder - ( lineWidth - VIC2_Render::outerUnscaledWidth ), 13 );

	prevCpuBarLines = int ( fullLines ) + 1;
}
//-----------------------------------------------------------------------------

void GUI_CRT::setVoiceRegs ( const uint8_t* regs, const int count )
{
	// Decoding mirrors GUI_ChipFrequencyLines::updateState; each older frame
	// sits 32 bytes before the newer one
	freqFramesUsed = std::min ( count, int ( freqFrames.size () ) );
	freqDataChanged = true;

	const auto	clockspeed = overlay.getSettings ().isNTSC ? SID::NTSC_CLOCK : SID::PAL_CLOCK;

	constexpr auto pow3 = [] ( const float a )
	{
		return a * a * a;
	};

	for ( auto frame = 0; frame < freqFramesUsed; ++frame, regs -= 32 )
	{
		const auto	filterMode = uint8_t ( ( regs[ 0x18 ] >> 4 ) & 0x7 );
		auto		routing = uint8_t ( regs[ 0x17 ] & 7 );
		auto		muted = uint8_t ( ( regs[ 0x18 ] & 0x80 ) >> 4 );

		auto	voiceVolOffset = 0x1d;

		for ( auto registerOffset = 0; auto& m : freqFrames[ size_t ( frame ) ] )
		{
			const auto	filtered = ( routing & 1 ) && filterMode;
			m.colIdx = uint8_t ( int ( filtered ) + ( muted & 2 ) );

			m.x = -1;
			const auto	pitch = uint16_t ( ( regs[ registerOffset + 0x01 ] << 8 ) + regs[ registerOffset + 0x00 ] );
			if ( pitch && stripW >= barWidth )
			{
				const auto	norm = UI::fft::freqToNormalized ( pitch * clockspeed );
				if ( norm >= 0.0f && norm < 1.0f )
					m.x = int16_t ( std::min ( int ( norm * float ( stripW ) ), stripW - barWidth ) );
			}

			m.volume = 1.0f - pow3 ( 1.0f - regs[ voiceVolOffset ] * ( 1.0f / 255.0f ) );

			registerOffset += SID::REGISTER_VOICE_DELTA;
			++voiceVolOffset;

			routing >>= 1;
			muted >>= 1;
		}
	}
}
//-----------------------------------------------------------------------------

void GUI_CRT::reloadPlayerLayout ()
{
	loadFrequencyColors ();
	loadPlayerLayout ( isBasicScreen );
}
//-----------------------------------------------------------------------------

void GUI_CRT::loadPlayerLayout ( const bool bootScreen )
{
	// The renderer may still point at the old font bits, and a new layout can
	// move the overlays: next pass draws everything
	vicRender.setCustomCharset ( nullptr );
	vicRender.invalidate ();
	prevBarSpans.clear ();

	composeDirty = true;
	layout.valid = false;

	if ( const auto name = pickLayoutFile ( bootScreen ); name.isNotEmpty () )
		loadLayoutFile ( name, layout );
}
//-----------------------------------------------------------------------------

juce::String GUI_CRT::pickLayoutFile ( const bool bootScreen )
{
	// The boot screens ship in the pak, the preferred one is always there
	if ( bootScreen )
		return juce::SharedResourcePointer<Preferences> ()->get<juce::String> ( "player/boot-screen" ) + ".petmate";

	juce::StringArray	pool;
	for ( const auto& file : datasource::listFiles ( "C64 Screens", false, "*.petmate" ) )
		if ( ! file.startsWithIgnoreCase ( "Basic" ) )
			pool.add ( file );

	if ( pool.isEmpty () )
		return {};

	// The preferred player screen wins while it exists; "Random" or a stale
	// name falls through to the random pick
	if ( const auto pick = juce::SharedResourcePointer<Preferences> ()->get<juce::String> ( "player/player-screen" ) + ".petmate"; pool.contains ( pick, true ) )
		return pick;

	return pool[ juce::Random::getSystemRandom ().nextInt ( pool.size () ) ];
}
//-----------------------------------------------------------------------------

bool GUI_CRT::loadLayoutFile ( const juce::String& filename, playerLayout& out )
{
	out = {};

	const auto	parsed = juce::JSON::parse ( datasource::loadText ( "C64 Screens/" + filename ) );

	const auto	frame = parsed[ "framebufs" ][ 0 ];

	if ( int ( frame[ "width" ] ) != VIC2_Render::textColumns || int ( frame[ "height" ] ) != VIC2_Render::textRows )
		return false;

	out.screenCol = uint8_t ( int ( frame[ "backgroundColor" ] ) & 0xF );
	out.borderCol = uint8_t ( int ( frame[ "borderColor" ] ) & 0xF );

	// upper/lower pick the ROM half; anything else names a custom font, which
	// brings its own full set (and keeps the case-preserving text mapping)
	const auto	charset = frame[ "charset" ].toString ();
	out.controlByte = charset == "upper" ? uint8_t ( 0x15 ) : uint8_t ( 0x17 );

	if ( charset != "upper" && charset != "lower" )
	{
		auto	bits = parsed[ "customFonts" ][ juce::Identifier ( charset ) ][ "font" ][ "bits" ];

		if ( auto* arr = bits.getArray (); arr && arr->size () == 2048 )
			for ( const auto& b : *arr )
				out.customFont.push_back ( uint8_t ( int ( b ) ) );
	}

	const auto	rows = frame[ "framebuf" ];

	for ( auto y = 0; y < VIC2_Render::textRows; ++y )
	{
		const auto	row = rows[ y ];

		for ( auto x = 0; x < VIC2_Render::textColumns; ++x )
		{
			const auto	cell = row[ x ];
			const auto	offset = y * VIC2_Render::textColumns + x;

			out.screen[ offset ] = uint8_t ( int ( cell[ "code" ] ) );
			out.color[ offset ] = uint8_t ( int ( cell[ "color" ] ) & 0xF );
		}
	}

	// Placeholder fields: a reverse-video run whose leading letters name a
	// known tag marks where the runtime pokes that value, as wide as the run,
	// in the color it was drawn in. Unknown reverse runs stay art
	static constexpr const char*	knownTags[] =
	{
		"NAME", "AUTHOR", "RELEASE", "TUNE", "MODEL", "CLOCK", "SPEED",
		"LOAD", "INIT", "PLAY", "TIME", "LEN", "BAR", "VER", "FREQ",
		"FILE", "FILE64", "FILE128",
	};

	for ( auto y = 0; y < VIC2_Render::textRows; ++y )
	{
		for ( auto x = 0; x < VIC2_Render::textColumns; )
		{
			const auto	offset = y * VIC2_Render::textColumns + x;

			if ( out.screen[ offset ] < 128 )
			{
				++x;
				continue;
			}

			// The tag from the run's leading reverse letters (both charset
			// cases) and digits, then the field is however much reverse space
			// follows, so touching fields stay separate whatever their colors
			std::string	tag;

			auto	end = x;
			while ( end < VIC2_Render::textColumns )
			{
				const auto	code = out.screen[ y * VIC2_Render::textColumns + end ];
				const auto	sc = uint8_t ( code - 128 );

				if ( code < 128 )
					break;

				if ( sc >= 1 && sc <= 26 )
					tag += char ( 'A' + sc - 1 );
				else if ( sc >= 65 && sc <= 90 )
					tag += char ( sc );
				else if ( sc >= '0' && sc <= '9' )
					tag += char ( sc );
				else
					break;

				++end;
			}

			// PETSCII has two identical-looking solid blocks, accept both
			while ( end < VIC2_Render::textColumns
					&& ( out.screen[ y * VIC2_Render::textColumns + end ] == 160 || out.screen[ y * VIC2_Render::textColumns + end ] == 224 ) )
				++end;

			if ( ! tag.empty () && std::ranges::find ( knownTags, tag ) != std::end ( knownTags ) )
			{
				out.fields.emplace ( tag, playerLayout::field { y, x, end - x, uint8_t ( out.color[ offset ] & 0xF ) } );
				std::fill_n ( out.screen + offset, end - x, uint8_t ( 32 ) );
			}

			x = std::max ( end, x + 1 );
		}
	}

	// The FREQ area spans its row plus the two below; whatever was drawn
	// there marks the area in the editor and goes blank here
	if ( const auto it = out.fields.find ( "FREQ" ); it != out.fields.end () )
	{
		const auto&	f = it->second;

		for ( auto row = f.row + 1; row <= f.row + 2 && row < VIC2_Render::textRows; ++row )
			std::fill_n ( out.screen + row * VIC2_Render::textColumns + f.col, f.width, uint8_t ( 32 ) );
	}

	// The sidecar json next to the layout defines its washes and raster bars
	parseSidecar ( juce::JSON::parse ( datasource::loadText ( "C64 Screens/" + filename.upToLastOccurrenceOf ( ".", false, false ) + ".json" ) ), out );

	out.valid = true;

	return true;
}
//-----------------------------------------------------------------------------

namespace
{
	// A sidecar color: a palette index, or a name in vic2 order
	[[ nodiscard ]] uint8_t parseSidecarColor ( const juce::var& c )
	{
		static const juce::StringArray	colorNames =
		{
			"black", "white", "red", "cyan", "purple", "green", "blue", "yellow",
			"orange", "brown", "light_red", "dark_grey", "grey", "light_green",
			"light_blue", "light_grey",
		};

		if ( c.isString () )
			return uint8_t ( std::max ( 0, colorNames.indexOf ( c.toString ().toLowerCase ().replaceCharacter ( ' ', '_' ) ) ) );

		return uint8_t ( int ( c ) & 0xF );
	}

	[[ nodiscard ]] std::vector<uint8_t> parseSidecarColors ( const juce::var& src )
	{
		std::vector<uint8_t>	out;

		if ( auto* colors = src[ "colors" ].getArray () )
			for ( const auto& c : *colors )
				out.push_back ( parseSidecarColor ( c ) );

		return out;
	}

	constexpr const char*	freqStates[] = { "on", "filtered", "muted" };

	// Fills the per-state fade ramps from a "frequency" object, absent
	// states come back empty
	void parseFrequencyRamps ( const juce::var& freq, std::vector<uint8_t>* ramps )
	{
		for ( auto i = 0; i < int ( std::size ( freqStates ) ); ++i )
			ramps[ i ] = parseSidecarColors ( freq[ freqStates[ i ] ] );
	}

	// The default frequency-strip fade ramps in the sidecar color format,
	// dark to bright
	void loadFrequencyColors ()
	{
		parseFrequencyRamps ( juce::JSON::parse ( datasource::loadText ( "C64 Screens/Frequency colors.json" ) )[ "frequency" ], fadeRamp );

		for ( auto i = 0; i < int ( std::size ( freqStates ) ); ++i )
			if ( fadeRamp[ i ].empty () )
				Z_ERR ( "Frequency colors.json: \"" << freqStates[ i ] << "\" has no colors" );
	}
}
//-----------------------------------------------------------------------------

void GUI_CRT::parseSidecar ( const juce::var& sidecar, playerLayout& out )
{
	// Per-screen frequency-ramp overrides
	parseFrequencyRamps ( sidecar[ "frequency" ], out.freqRamps );

	// Progress-bar colors, absent keys keep the defaults
	if ( const auto bar = sidecar[ "bar" ]; bar.isObject () )
	{
		if ( const auto v = bar[ "playhead" ]; ! v.isVoid () )		out.barPlayhead = parseSidecarColor ( v );
		if ( const auto v = bar[ "rendered" ]; ! v.isVoid () )		out.barRendered = parseSidecarColor ( v );
		if ( const auto v = bar[ "background" ]; ! v.isVoid () )	out.barBackground = parseSidecarColor ( v );
	}

	// Border raster bars
	if ( auto* barArray = sidecar[ "rasterbars" ].getArray () )
	{
		for ( const auto& src : *barArray )
		{
			playerLayout::rasterbar	bar;

			bar.colors = parseSidecarColors ( src );
			if ( bar.colors.empty () )
				continue;

			bar.y = int ( src[ "y" ] );
			bar.amplitude = int ( src[ "amplitude" ] );

			if ( const auto v = src[ "period" ]; ! v.isVoid () )
				bar.period = std::max ( 1, int ( v ) );

			if ( const auto v = src[ "phase" ]; ! v.isVoid () )
				bar.phase = float ( double ( v ) );

			out.rasterbars.push_back ( std::move ( bar ) );
		}
	}

	auto* washArray = sidecar[ "washes" ].getArray ();
	if ( ! washArray )
		return;

	for ( const auto& src : *washArray )
	{
		const auto	rect = src[ "rect" ];

		const auto	x = int ( rect[ 0 ] );
		const auto	y = int ( rect[ 1 ] );
		const auto	w = int ( rect[ 2 ] );
		const auto	h = int ( rect[ 3 ] );

		if ( x < 0 || y < 0 || w < 1 || h < 1 || x + w > VIC2_Render::textColumns || y + h > VIC2_Render::textRows )
			continue;

		playerLayout::wash	wash;

		wash.colors = parseSidecarColors ( src );
		if ( wash.colors.empty () )
			continue;

		// repeat stretches every color over that many steps (the full cycle
		// grows with it, the travel speed stays)
		if ( const auto v = src[ "repeat" ]; int ( v ) > 1 )
		{
			std::vector<uint8_t>	stretched;
			for ( const auto color : wash.colors )
				stretched.insert ( stretched.end (), size_t ( int ( v ) ), color );

			wash.colors = std::move ( stretched );
		}

		if ( const auto v = src[ "speed" ]; ! v.isVoid () )
			wash.speed = std::max ( 1, int ( v ) );

		if ( const auto v = src[ "offset" ]; ! v.isVoid () )
			wash.offset = int ( v );

		if ( const auto v = src[ "pause" ]; ! v.isVoid () )
			wash.pause = std::max ( 0, int ( v ) );

		auto	stepSize = 1;
		if ( const auto v = src[ "step-size" ]; ! v.isVoid () )
			stepSize = std::max ( 1, int ( v ) );

		// A boxy rect walks its perimeter, strips sweep along their length
		auto	direction = src[ "direction" ].toString ();
		if ( direction.isEmpty () )
			direction = w > 1 && h > 2 ? "clock" : w > 1 ? "right" : "down";

		auto add = [ &wash ] ( const int col, const int row, const int pos )
		{
			wash.cells.push_back ( { int16_t ( row * VIC2_Render::textColumns + col ), int16_t ( pos ) } );
		};

		if ( direction == "clock" || direction == "counter" )
		{
			// The ring, clockwise from the top-left corner
			auto	pos = 0;

			if ( h == 1 )
				for ( auto c = 0; c < w; ++c )
					add ( x + c, y, pos++ );
			else if ( w == 1 )
				for ( auto r = 0; r < h; ++r )
					add ( x, y + r, pos++ );
			else
			{
				for ( auto c = 0; c < w; ++c )
					add ( x + c, y, pos++ );
				for ( auto r = 1; r < h; ++r )
					add ( x + w - 1, y + r, pos++ );
				for ( auto c = w - 2; c >= 0; --c )
					add ( x + c, y + h - 1, pos++ );
				for ( auto r = h - 2; r >= 1; --r )
					add ( x, y + r, pos++ );
			}

			if ( direction == "counter" )
				for ( auto& c : wash.cells )
					c.pos = int16_t ( pos - 1 - c.pos );
		}
		else
		{
			const auto	vertical = direction == "up" || direction == "down";
			const auto	reversed = direction == "left" || direction == "up";

			for ( auto r = 0; r < h; ++r )
				for ( auto c = 0; c < w; ++c )
				{
					const auto	pos = vertical ? r : c;
					const auto	length = vertical ? h : w;

					add ( x + c, y + r, reversed ? length - 1 - pos : pos );
				}
		}

		// step-size walk-consecutive cells share a color step; spanning the
		// whole run flashes it uniformly. Applied last, after any direction
		// reversal of the walk positions
		if ( stepSize > 1 )
			for ( auto& c : wash.cells )
				c.pos = int16_t ( c.pos / stepSize );

		out.washes.push_back ( std::move ( wash ) );
	}
}
//-----------------------------------------------------------------------------

void GUI_CRT::composePlayerScreen ( const std::string& currentTime, const std::string& lengthStr )
{
	auto&	vic = vicRender;

	// Nothing below space may reach the screen; backtick is placeText's
	// cursor escape
	auto printableOnly = [] ( std::string s )
	{
		std::erase_if ( s, [] ( const unsigned char c ) { return c < 32 || c == '`'; } );
		return s;
	};

	vic.screenCol = layout.screenCol;
	vic.borderCol = layout.borderCol;
	vic.controlByte = layout.controlByte;
	vic.setCustomCharset ( layout.customFont.empty () ? nullptr : layout.customFont.data () );

	std::copy_n ( layout.screen, std::size ( layout.screen ), vic.screenBuffer );
	std::copy_n ( layout.color, std::size ( layout.color ), vic.colorBuffer );

	auto pokeField = [ this, &vic ] ( const char* tag, const std::string& value, const bool rightAlign = false )
	{
		if ( value.empty () )
			return;

		// Every run of the tag gets the value, each clipped to its own width
		const auto	[ begin, end ] = layout.fields.equal_range ( tag );
		for ( auto it = begin; it != end; ++it )
		{
			const auto&	f = it->second;

			auto	clipped = value;
			if ( int ( clipped.size () ) > f.width )
				clipped.resize ( size_t ( f.width ) );

			vic.placeText ( f.col + ( rightAlign ? f.width - int ( clipped.size () ) : 0 ), f.row, f.color, clipped.c_str () );
		}
	};

	// The tune fields stay quiet on the boot screen
	if ( isPlayerUI )
	{
		const auto	title = printableOnly ( stringutils::utf8toExtendedASCII ( sidInfoStr.title ) );

		pokeField ( "NAME", title.empty () ? printableOnly ( sidname.toStdString () ) : title );

		// The filename without extension, cut to the 16 chars CBM DOS allows
		auto	file = printableOnly ( stringutils::utf8toExtendedASCII ( sidname.upToLastOccurrenceOf ( ".", false, false ).toStdString () ) );
		if ( file.size () > 16 )
			file.resize ( 16 );

		pokeField ( "FILE", file );

		// Ready-made load-line tails, the layout draws the opening LOAD"/RUN"
		pokeField ( "FILE64", file + "\",8,1" );
		pokeField ( "FILE128", file + "\" FROM 8" );
		pokeField ( "AUTHOR", printableOnly ( stringutils::utf8toExtendedASCII ( sidInfoStr.author ) ) );
		pokeField ( "RELEASE", printableOnly ( stringutils::utf8toExtendedASCII ( sidInfoStr.released ) ) );
		pokeField ( "TUNE", std::to_string ( sidInfoStr.currentSong ) + " of " + std::to_string ( sidInfoStr.numSongs ) );

		std::string	models;
		for ( const auto& m : sidInfoStr.model )
			models += ( models.empty () ? "" : "+" ) + m;

		// The clock has its own field, drop it from the speed string
		auto	speed = sidInfoStr.speed;
		if ( const auto paren = speed.find ( " (" ); paren != std::string::npos )
			speed.resize ( paren );

		pokeField ( "MODEL", printableOnly ( models ) );
		pokeField ( "CLOCK", printableOnly ( sidInfoStr.clock ) );
		pokeField ( "SPEED", printableOnly ( speed ) );

		char	buf[ 24 ];

		std::snprintf ( buf, sizeof ( buf ), "$%04X-%04X", sidInfoStr.c64LoadAddress, uint16_t ( sidInfoStr.c64LoadAddress + sidInfoStr.c64DataLength - 1 ) );
		pokeField ( "LOAD", buf );

		std::snprintf ( buf, sizeof ( buf ), "$%04X", sidInfoStr.c64InitAddress );
		pokeField ( "INIT", buf );

		std::snprintf ( buf, sizeof ( buf ), "$%04X", sidInfoStr.c64PlayAddress );
		pokeField ( "PLAY", buf );

		pokeField ( "TIME", currentTime, true );
		pokeField ( "LEN", lengthStr );
	}

	pokeField ( "VER", juce::String ( ProjectInfo::versionString ).toStdString () );

	// The frequency strip lives wherever the FREQ field was drawn (its row
	// plus the two below, the axis squeezed to the run's width), the
	// progress bar wherever BAR was, in its color; both idle on the boot
	// screen
	stripW = 0;
	barCells = 0;

	if ( isPlayerUI )
	{
		if ( const auto it = layout.fields.find ( "FREQ" ); it != layout.fields.end () )
		{
			const auto&	f = it->second;

			stripX = f.col * 8;
			stripY = f.row * 8;
			stripW = f.width * 8;
		}

		if ( const auto it = layout.fields.find ( "BAR" ); it != layout.fields.end () )
		{
			const auto&	f = it->second;

			barRow = f.row;
			barCol = f.col;
			barCells = f.width;
			barColor = f.color;
		}
	}
}
//-----------------------------------------------------------------------------

VIC2_Render::renderStats GUI_CRT::updatePlayerScreen ()
{
	auto&	vic = vicRender;

	//
	// A hand-drawn layout: its base buffers plus the tagged fields
	//
	if ( layout.valid )
	{
		// Mirrors the footer: current time, total length or remaining
		std::string	currentTime, lengthStr;

		if ( isPlayerUI )
		{
			currentTime = SID::convertTimeToString ( playTimeMS ).toStdString ();

			lengthStr = "-";
			if ( playLengthMS > 0 && playLengthMS != INT32_MAX )
			{
				if ( preferences->get<bool> ( "player/show-length" ) )
					lengthStr = SID::convertTimeToString ( playLengthMS ).toStdString ();
				else if ( playTimeMS < playLengthMS )
					lengthStr = "-" + SID::convertTimeToString ( playLengthMS - playTimeMS ).toStdString ();
			}
		}

		// The compose is the string-churning part: skipped while the layout,
		// the tune and the displayed times all hold still
		if ( composeDirty || currentTime != composedTime || lengthStr != composedLength )
		{
			composeDirty = false;
			composedTime = currentTime;
			composedLength = lengthStr;

			composePlayerScreen ( currentTime, lengthStr );
		}

		// Color-RAM washes over whatever sits there, fields included; the
		// pattern travels toward the walk direction. The clock runs in
		// frames: the pause appends a rest of that many frames to the cycle,
		// independent of the speed, where the base colors show (restored
		// here, the compose does not refresh the buffers every frame)
		for ( const auto& w : layout.washes )
		{
			const auto	patternFrames = int ( w.colors.size () ) * w.speed;
			const auto	period = patternFrames + w.pause;
			const auto	t = washPhase + w.offset * w.speed;

			for ( const auto& c : w.cells )
			{
				const auto	frame = ( ( c.pos * w.speed - t ) % period + period ) % period;

				vic.colorBuffer[ c.offset ] = frame < patternFrames ? w.colors[ size_t ( frame / w.speed ) ] : layout.color[ c.offset ];
			}
		}

		const auto	stats = vic.renderScreen ();

		// A full pass paints over the pixel bar, force its redraw
		if ( stats.full )
			lastPlayedPx = -1;

		return stats;
	}

	//
	// Unloadable screen data: a blank screen, the overlays idle
	//
	vic.setCustomCharset ( nullptr );

	vic.screenCol = vic2::black;
	vic.borderCol = vic2::black;
	vic.controlByte = 0x15;			// uppercase / graphics charset

	std::fill_n ( vic.screenBuffer, VIC2_Render::textColumns * VIC2_Render::textRows, uint8_t ( 32 ) );
	std::fill_n ( vic.colorBuffer, VIC2_Render::textColumns * VIC2_Render::textRows, uint8_t ( vic2::light_grey ) );

	stripW = 0;
	barCells = 0;

	return vic.renderScreen ();
}
//-----------------------------------------------------------------------------

int GUI_CRT::progressFillPx ( const int timeMS ) const
{
	const auto	barWidth = barCells * 8;

	// Rounded: the emulated render clock ends a fraction short of the length,
	// flooring would leave the bar one pixel shy forever
	return std::clamp ( int ( ( int64_t ( timeMS ) * barWidth + playLengthMS / 2 ) / playLengthMS ), 0, barWidth );
}
//-----------------------------------------------------------------------------

bool GUI_CRT::drawProgressBar ( uint8_t* dst )
{
	if ( barCells <= 0 || playLengthMS <= 0 || playLengthMS == INT32_MAX )
		return false;

	const auto	played = progressFillPx ( playTimeMS );
	const auto	rendered = progressFillPx ( playRenderMS );

	// Nothing moved a full pixel, the bar in the buffer is still right
	if ( played == lastPlayedPx && rendered == lastRenderedPx )
		return false;

	lastPlayedPx = played;
	lastRenderedPx = rendered;

	// The whole bar repaints as pixels: played, pre-rendered and background
	// spans, the playhead on the fill's leading edge
	const auto	barWidth = barCells * 8;

	const auto	left = VIC2_Render::unscaledBorderSizeX + barCol * 8;
	const auto	top = VIC2_Render::unscaledBorderSizeY + barRow * 8;

	auto fillColumns = [ & ] ( const int x, const int width, const uint8_t color )
	{
		if ( width <= 0 )
			return;

		auto	d = dst + top * VIC2_Render::outerUnscaledWidth + left + x;

		for ( auto line = 0; line < 8; ++line, d += VIC2_Render::outerUnscaledWidth )
			std::fill_n ( d, size_t ( width ), color );
	};

	fillColumns ( 0, played, barColor );
	fillColumns ( played, rendered - played, layout.barRendered );
	fillColumns ( rendered, barWidth - rendered, layout.barBackground );

	fillColumns ( std::min ( played, barWidth - 2 ), 2, layout.barPlayhead );

	return true;
}
//-----------------------------------------------------------------------------

void GUI_CRT::drawFrequencyStrip ( uint8_t* dst ) const
{
	constexpr auto	numFrames = int ( std::tuple_size_v<decltype ( freqFrames )> );

	const auto	stripTop = VIC2_Render::unscaledBorderSizeY + stripY;
	const auto	stripLeft = VIC2_Render::unscaledBorderSizeX + stripX;

	// The sprites' stage: clear to the black the fade ramps were drawn for
	for ( auto line = 0; line < stripHeight; ++line )
		std::fill_n ( dst + ( stripTop + line ) * VIC2_Render::outerUnscaledWidth + stripLeft, stripW, uint8_t ( vic2::black ) );

	// Oldest first, so a held note's fresh marker wins the overlap: sprite 0
	// carries the newest frame and sits in front by hardware priority
	for ( auto age = freqFramesUsed - 1; age >= 0; --age )
	{
		const auto	ageDecay = float ( numFrames - age ) / float ( numFrames );

		for ( auto band = 0; const auto& m : freqFrames[ size_t ( age ) ] )
		{
			// One line down centers the 6px bar in its 8px band
			const auto	bandTop = stripTop + 1 + band++ * bandPitch;

			if ( m.x < 0 || m.x > stripW - barWidth )
				continue;

			// Quieter than 1/32 draws nothing
			const auto	brightness = UI::fft::pow2 ( ageDecay * m.volume );
			if ( brightness < 1.0f / 32.0f )
				continue;

			const auto	state = fadeRampForState[ m.colIdx ];
			const auto&	over = layout.freqRamps[ state ];

			const auto&	ramp = over.empty () ? fadeRamp[ state ] : over;
			if ( ramp.empty () )
				continue;

			const auto	color = ramp[ std::min ( size_t ( brightness * float ( ramp.size () ) ), ramp.size () - 1 ) ];

			auto	d = dst + bandTop * VIC2_Render::outerUnscaledWidth + stripLeft + m.x;

			for ( auto line = 0; line < barHeight; ++line, d += VIC2_Render::outerUnscaledWidth )
				std::fill_n ( d, barWidth, color );
		}
	}
}
//-----------------------------------------------------------------------------

void GUI_CRT::renderCRT ( const bool generate )
{
	lastWasGenerated = generate;

	auto	vic2Settings = settingsPanel.getVIC2SettingsFromPreferences ();
	vicRender.setSettings ( vic2Settings );

	isBasicScreen = generate && sidname.isEmpty ();
	isPlayerUI = generate && sidname.isNotEmpty ();

	// Generated screens are hand-drawn layouts: the boot pool with no tune,
	// the player pool otherwise
	if ( generate )
		updatePlayerScreen ();

	auto	settings = overlay.getSettings ();
	settings.isNTSC = vic2Settings.standard == VIC2_Render::settings::NTSC;

	updateCRTPalette ( vic2Settings );

	overlay.setSettings ( settings );
	overlay.setIndexTextureSource ( vicRender.getCRT () );

	settingsPanel.updateCRTsettingsUI ();
}
//-----------------------------------------------------------------------------

void GUI_CRT::updateOverlayCRTSettings ()
{
	auto	settings = settingsPanel.getCRTEmulationSettingsFromPreferences ();

	// VIC2 settings that affect CRT emulation
	{
		const auto	vic2Settings = settingsPanel.getVIC2SettingsFromPreferences ();

		settings.isNTSC = vic2Settings.standard == VIC2_Render::settings::NTSC;
		settings.crtEmulation = ! vic2Settings.raw;

		updateCRTPalette ( vic2Settings );
	}

	overlay.setSettings ( settings );
}
//-----------------------------------------------------------------------------

void GUI_CRT::updateCRTPalette ( const VIC2_Render::settings& vic2Settings )
{
	if (	vic2Settings.needsNewPalette ( curVicSettings )
		 ||	yuv_yiq.empty () )
	{
		yuv_yiq = colo.generateYUV_YIQ ( vic2Settings.firstLuma, vic2Settings.warmth );
		overlay.setLumaChromaPalette ( yuv_yiq );

		curVicSettings = vic2Settings;
	}
}
//-----------------------------------------------------------------------------

std::pair<juce::String, const int> GUI_CRT::findArtwork ( const juce::String& _sidname, const juce::String& indexStr )
{
	tuneArtwork = scrshot->getScreenshots ( _sidname.toStdString () );

	overlay.setNumCRTpages ( int ( tuneArtwork.size () ) );

	if ( tuneArtwork.empty () )
		return {};

	auto	index = ScreenshotLookup::getDefaultScreenshotIndex ( tuneArtwork );
	if ( indexStr.isNotEmpty () )
	{
		const auto	idxStr = indexStr.toStdString ();
		auto findWithHint = [ &idxStr ] (const std::string& str) -> bool
		{
			if ( idxStr == str )
				return true;

			const auto	hint = imageutils::hintFromFilename ( str );
			return idxStr == hint.name + hint.extension;
		};

		if ( auto it = std::ranges::find_if ( tuneArtwork, findWithHint ); it != tuneArtwork.end () )
			index = int ( std::distance ( tuneArtwork.begin (), it ) );
	}

	overlay.setCRTPage ( index );

	return { tuneArtwork[ size_t ( index ) ], index };
}
//-----------------------------------------------------------------------------

bool GUI_CRT::isInterestedInFileDrag ( const juce::StringArray& files )
{
	if ( ! buildinfo::isDeveloperMode () )
		return false;

	if ( textutils::getFilteredStrings ( files, { ".png" } ).size () )
		return true;

	return false;
}
//-----------------------------------------------------------------------------

void GUI_CRT::filesDropped ( const juce::StringArray& files, int /*x*/, int /*y*/ )
{
	if ( ! buildinfo::isDeveloperMode () )
		return;

	const msg::AddScreenshots	e { textutils::getFilteredStrings ( files, { ".png" } ) };

	if ( e.files.isEmpty () )
		return;

	e.send ();
}
//-----------------------------------------------------------------------------

bool GUI_CRT::isInterestedInTextDrag ( const juce::String& text )
{
	return buildinfo::isDeveloperMode () && textutils::isUrlWithExtension ( text, { ".png" } );
}
//-----------------------------------------------------------------------------

void GUI_CRT::textDropped ( const juce::String& text, int /*x*/, int /*y*/ )
{
	if ( ! buildinfo::isDeveloperMode () )
		return;

	msg::DownloadScreenshot { text.trim () }.send ();
}
//-----------------------------------------------------------------------------

void GUI_CRT::userCRTContentChanged ( const juce::String& relPath, const gin::FileSystemWatcher::FileSystemEvent event )
{
	// The same file addressed through the factory tree, which is the only way
	// lime knows it; the content loader routes it back to the user file
	const auto	nominal = datasource::getCRTRoot ().getChildFile ( relPath );

	// Live tweak of an existing file: feed lime's own hot-reload path (profile
	// yml re-parse, texture reloads with dependency tracking)
	if ( event == gin::FileSystemWatcher::fileUpdated )
	{
		overlay.fileChanged ( nominal, event );
		return;
	}

	// Files appeared or disappeared: the pick lists change, and the files
	// behind the active overlay/mask may have come or gone, so re-pull them
	// through the loader
	overlay.rescanOverlays ();
	settingsPanel.refreshCRTPickLists ();

	updateOverlayCRTSettings ();
	overlay.loadOverlayProfile ( settingsPanel.currentOverlayName () );
	overlay.updateOverlay ();
}
//-----------------------------------------------------------------------------
