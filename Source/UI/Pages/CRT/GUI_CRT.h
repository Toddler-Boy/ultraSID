#pragma once

#include <JuceHeader.h>

#include "libSidplayEZ/src/EZ/SidTuneInfoEZ.h"

#include "ultra-shared/UI/GUI_CRTSettings.h"
#include "ultra-shared/Video/VIC2_Render.h"

#include "App/ScreenshotLookup.h"
#include "Audio/sid-constants.h"
#include "Config/Preferences.h"
#include "UI/ComponentFactory.h"

#include "GUI_Overlay.h"

//-----------------------------------------------------------------------------

class GUI_CRT final
	: public juce::Component
	, public juce::FileDragAndDropTarget
	, public juce::TextDragAndDropTarget
{
public:
	GUI_CRT ();

	// juce::Component
	void resized () override;
	void mouseWheelMove ( const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel ) override;

	// juce::FileDragAndDropTarget
	bool isInterestedInFileDrag ( const juce::StringArray& files ) override;
	void filesDropped ( const juce::StringArray& files, int x, int y ) override;

	// juce::TextDragAndDropTarget
	bool isInterestedInTextDrag ( const juce::String& text ) override;
	void textDropped ( const juce::String& text, int x, int y ) override;

	// this
	void setStrings ( const SidTuneInfoEZ& src );
	void reloadOverlayProfile ()	{	overlay.reloadOverlayProfile ();	}

	// A file under the user Overlays / CRT Masks folders changed (relPath is
	// relative to the user root); hot-reload live tweaks or rescan pick lists
	void userCRTContentChanged ( const juce::String& relPath, gin::FileSystemWatcher::FileSystemEvent event );

	// A file under the user CRT Presets folder changed: rebuild the pick lists
	void userCRTPresetsChanged ()	{	settingsPanel.refreshCRTPickLists ();	}

	void loadGameArtwork ( const juce::String& sidname, const juce::String& index = "" );
	void loadGameArtwork ( const int index );

	// The naked file behind the shown artwork, for developer curation
	[[ nodiscard ]] juce::File getLastLoadedFile ();
	[[ nodiscard ]] int getGameArtworkIndex () const	{	return tuneArtIndex;	}

	[[ nodiscard ]] bool areSettingsVisible () const	{ return settingsVisible;	}
	void showSettings ( const bool visible );
	void setBackgroundColour ( const juce::Colour& bckCol );

	void timerUpdate ( const float secondsPassed, const uint16_t cpuCycles );

	// Chip-0 register snapshots for the frequency strip; regs points at the
	// newest 32-byte frame, count is the play position's absolute frame index
	void setVoiceRegs ( const uint8_t* regs, const int count );

	// Playback position for the player screen's time display and progress bar;
	// renderMS is how far the tune has been pre-rendered
	void setPlaybackTime ( const int timeMS, const int lengthMS, const int renderMS )
	{
		playTimeMS = timeMS;
		playLengthMS = lengthMS;
		playRenderMS = renderMS;
	}

	// A file in Data/C64 Screens changed (developer hot-reload)
	void reloadPlayerLayout ();

	// The boot- and player-screen preferences changed: re-pick while the
	// matching screen is showing
	void bootScreenPickChanged ()	{	if ( isBasicScreen ) reloadPlayerLayout ();	}
	void playerScreenPickChanged ()	{	if ( isPlayerUI ) reloadPlayerLayout ();	}

	void setCRTPage ( const int page )	{	overlay.setCRTPage ( page );	}
	[[ nodiscard ]] int getCRTPage () const				{	return overlay.getCRTPage ();	}

	// Draws the GL-rendered CRT into a software snapshot of top, which skips it
	void paintIntoSnapshot ( juce::Image& snapshot, juce::Component& top );

private:
	// this
	void renderCRT ( const bool generate = false );

	// The panel's settings plus the VIC2-derived fields, pushed into the emulation
	void updateOverlayCRTSettings ();

	void updateCRTPalette ( const VIC2_Render::settings& vic2Settings );

	[[ nodiscard ]] std::pair<juce::String, const int> findArtwork ( const juce::String& sidname, const juce::String& index );

	// Feed the shown artwork to the VIC2 renderer; false when there is none
	[[ nodiscard ]] bool loadArtworkImage ();

	SidTuneInfoEZ		sidInfoStr;
	juce::String		sidname;
	juce::String		lastLoadedName;		// Relative to Screenshots/

	const colodore				colo;
	colodore::shaderPalette		yuv_yiq;
	VIC2_Render::settings		curVicSettings;

	bool	lastWasGenerated = false;
	bool	lastFirstLuma = false;

	bool	isBasicScreen = false;
	bool	isPlayerUI = false;

	juce::SharedResourcePointer<Preferences>		preferences;
	juce::SharedResourcePointer<ScreenshotLookup>	scrshot;

	int							tuneArtIndex = 0;
	std::vector<std::string>	tuneArtwork;

	VIC2_Render		vicRender { true };
	GUI_Overlay		overlay;
	float			timePassed = 0.0f;

public:
	// A hand-drawn Petmate screen from Data/C64 Screens: the base buffers plus
	// the tagged placeholder fields the runtime pokes its values into
	struct playerLayout
	{
		struct field
		{
			int		row = 0, col = 0, width = 0;
			uint8_t	color = 0;
		};

		// A color-RAM wash from the sidecar json: buffer cells in walk order
		// with their position along the cycle
		struct wash
		{
			struct cell
			{
				int16_t	offset = 0;
				int16_t	pos = 0;
			};

			std::vector<cell>		cells;
			std::vector<uint8_t>	colors;

			int		speed = 2;
			int		offset = 0;
			int		pause = 0;		// rest frames between cycles, drawn colors show
		};

		// A border-color raster bar from the sidecar json, swinging on a
		// sine; one color per raster line
		struct rasterbar
		{
			std::vector<uint8_t>	colors;

			int		y = 0;				// center line, relative to the inner screen
			int		amplitude = 0;
			int		period = 180;		// frames per full swing
			float	phase = 0.0f;
		};

		bool	valid = false;

		uint8_t		screen[ VIC2_Render::textColumns * VIC2_Render::textRows ];
		uint8_t		color[ VIC2_Render::textColumns * VIC2_Render::textRows ];
		uint8_t		screenCol = 0;
		uint8_t		borderCol = 0;
		uint8_t		controlByte = 0x17;

		std::vector<uint8_t>	customFont;		// full 2KB set when present

		std::multimap<std::string, field>	fields;	// a tag may appear on several runs
		std::vector<wash>				washes;
		std::vector<rasterbar>			rasterbars;

		// Frequency-strip fade-ramp overrides (on, filtered, muted), an
		// empty state falls back to the app default
		std::vector<uint8_t>	freqRamps[ 3 ];

		// Progress-bar colors, sidecar-overridable; the fill color rides
		// the BAR field's drawn color
		uint8_t	barPlayhead = vic2::light_green;
		uint8_t	barRendered = vic2::grey;
		uint8_t	barBackground = vic2::dark_grey;
	};

	// One layout file, sidecar included; false when it doesn't fit the screen.
	// Also serves the fallback-thumbnail renderer
	static bool loadLayoutFile ( const juce::String& filename, playerLayout& out );

	// A random pick: files named "Basic*" form the boot-screen pool for the
	// no-tune screen, everything else the player pool; empty when there is none
	[[ nodiscard ]] static juce::String pickLayoutFile ( const bool bootScreen );

private:
	// The text-based player screen: washes run and the buffers diff against
	// the pixels every frame, the compose itself only when its inputs moved
	VIC2_Render::renderStats updatePlayerScreen ();

	// The full compose: base buffers plus every tagged field value
	void composePlayerScreen ( const std::string& currentTime, const std::string& lengthStr );

	// Picks a random layout for the current mode into the live layout
	void loadPlayerLayout ( const bool bootScreen );

	// The washes and raster bars from a layout's sidecar json
	static void parseSidecar ( const juce::var& sidecar, playerLayout& out );

	// Draws erase their previous frame themselves and report whether they
	// touched the buffer, so unchanged frames skip the texture upload
	bool drawLayoutRasterbars ( uint8_t* dst, const bool force );
	void eraseCpuRasterBars ( uint8_t* dst );

	playerLayout	layout;

	void drawRasterBars ( uint8_t* dst, const uint16_t cpuCycles );

	// Frequency-strip markers from the last 8 register frames ([0] = newest),
	// drawn as the 8 sprites a real C64 could show there
	void drawFrequencyStrip ( uint8_t* dst ) const;

	int		playTimeMS = 0;
	int		playLengthMS = 0;
	int		playRenderMS = 0;
	int		washPhase = 0;

	// Compose gate: what the poked buffers were last built from
	bool		composeDirty = true;
	std::string	composedTime, composedLength;

	// Progress bar geometry, from the layout's BAR field; the bar renders
	// as pixels, not characters
	int		barRow = 0;
	int		barCol = 0;
	int		barCells = 0;
	uint8_t	barColor = 0;

	// Frequency strip area in inner-screen pixels, from the layout's FREQ
	// field; zero width = no strip
	int		stripX = 0;
	int		stripY = 0;
	int		stripW = 0;

	bool drawProgressBar ( uint8_t* dst );

	// The last drawn bar spans: no redraw until a boundary moves a pixel
	int		lastPlayedPx = -1;
	int		lastRenderedPx = -1;

	// The strip only redraws on fresh register data
	bool	freqDataChanged = true;

	// The border lines the bars covered last frame, for their self-erase
	std::vector<std::pair<int, int>>	prevBarSpans;
	int		prevCpuBarLines = 0;

	// Exact progress-bar fill width in pixels for a time, 0 .. bar width
	[[ nodiscard ]] int progressFillPx ( const int timeMS ) const;

	struct freqMarker
	{
		int16_t	x = -1;
		uint8_t	colIdx = 0;
		float	volume = 0.0f;
	};

	std::array<std::array<freqMarker, SID::numVoices>, 8>	freqFrames;
	int		freqFramesUsed = 0;

	// Show hide/settings
	bool	settingsVisible = false;

	// The shared settings panel; the page layout positions it by its
	// component name "settings"
	GUI_CRTSettings	settingsPanel;

	gin::LayoutSupport	crtLayout { *this, [] ( const juce::String& typeName ) { return componentFactory ( typeName ); } };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_CRT )
};
//-----------------------------------------------------------------------------
