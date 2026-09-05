#include <JuceHeader.h>

#include <algorithm>

#include "GUI_Settings.h"

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/ComponentUtils.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_AudioDeviceSelector.h"
#include "ultra-shared/UI/Components/GUI_ComboBox.h"
#include "ultra-shared/UI/Components/GUI_CRTSliderLabel.h"
#include "ultra-shared/UI/Components/GUI_Disabler.h"
#include "ultra-shared/UI/Components/GUI_SettingsChoice.h"
#include "ultra-shared/UI/Components/GUI_SettingsNumberEdit.h"
#include "ultra-shared/UI/Components/GUI_SettingsToggle.h"
#include "ultra-shared/UI/Components/GUI_ThemeSelector.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/ExportName.h"
#include "Config/Preferences.h"
#include "Helpers/Messages.h"
#include "UI/Components/GUI_EQCurve.h"
#include "UI/Components/GUI_SettingsLocation.h"
#include "UI/Components/GUI_SettingsText.h"
#include "UI/Components/GUI_SettingsUserData.h"

//-----------------------------------------------------------------------------

// The export-template token colors, shared by the help line and the preview
static int exportTokenColor ( const juce::juce_wchar token )
{
	switch ( token )
	{
		case 'T':	return UI::colors::exportTokenTitle;
		case 'A':	return UI::colors::exportTokenAuthor;
		case 'R':	return UI::colors::exportTokenRelease;
		case 'Y':	return UI::colors::exportTokenYear;
		case 'N':	return UI::colors::exportTokenNumber;
		case 'Q':	return UI::colors::exportTokenQuality;
		default:	return UI::colors::textMuted;
	}
}
//-----------------------------------------------------------------------------

// Colors the {X} tokens of the help text
static std::vector<GUI_ColorText::Segment> colorizeTokenHelp ( const juce::String& help )
{
	std::vector<GUI_ColorText::Segment>	segments;

	auto	rest = help;
	while ( rest.isNotEmpty () )
	{
		const auto	open = rest.indexOfChar ( '{' );
		const auto	close = open >= 0 ? rest.indexOfChar ( open, '}' ) : -1;

		if ( close < 0 )
		{
			segments.push_back ( { rest, UI::colors::textMuted } );
			break;
		}

		if ( open > 0 )
			segments.push_back ( { rest.substring ( 0, open ), UI::colors::textMuted } );

		const auto	token = rest.substring ( open, close + 1 );
		segments.push_back ( { token, exportTokenColor ( token[ 1 ] ), true } );

		rest = rest.substring ( close + 1 );
	}

	return segments;
}
//-----------------------------------------------------------------------------

// Splits a token-marked expansion into colored segments (see exportname::make)
static void appendMarkedSegments ( std::vector<GUI_ColorText::Segment>& segments, const std::string& marked )
{
	size_t	pos = 0;
	while ( pos < marked.size () )
	{
		if ( marked[ pos ] == exportname::markStart && pos + 1 < marked.size () )
		{
			auto	end = marked.find ( exportname::markEnd, pos );
			if ( end == std::string::npos )
				end = marked.size ();

			segments.push_back ( { juce::String::fromUTF8 ( marked.data () + pos + 2, int ( end - pos - 2 ) ), exportTokenColor ( marked[ pos + 1 ] ), true } );
			pos = end + 1;
		}
		else
		{
			auto	end = marked.find ( exportname::markStart, pos );
			if ( end == std::string::npos )
				end = marked.size ();

			segments.push_back ( { juce::String::fromUTF8 ( marked.data () + pos, int ( end - pos ) ), UI::colors::textMuted } );
			pos = end;
		}
	}
}
//-----------------------------------------------------------------------------

GUI_Settings::GUI_Settings ( juce::AudioDeviceManager& _adm )
	: adm ( _adm )
{
	setName ( "settings" );

	// Viewport
	settingsVP.setViewedComponent ( &settingsWrapper, false );
	addAndMakeVisible ( settingsVP );
}
//-----------------------------------------------------------------------------

GUI_Settings::~GUI_Settings ()
{
	// resized () may never have run, so the map can be empty
	if ( const auto ads = componentutils::findComponent<GUI_AudioDeviceSelector> ( "audio-device/output", componentMap ) )
		settings->set ( "output/device", ads->getCurrentOutputDevice () );
}
//-----------------------------------------------------------------------------

void GUI_Settings::resized ()
{
	const auto	vpPos = settingsVP.getViewPosition ();

	// The audio-enhancement tuning sliders exist in developer builds only
	layout.setConstant ( "dev", buildinfo::isDeveloperMode () ? 1 : 0 );

	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/pages/settings.json" } );

	settingsVP.setViewPosition ( vpPos );

	if ( componentMap.empty () )
	{
		componentutils::buildComponentMap ( componentMap, &settingsWrapper );

		// Set the audio device manager for the audio device selector component
		{
			auto	ads = componentutils::findComponent<GUI_AudioDeviceSelector> ( "audio-device/output", componentMap );

			// You need to create an GUI_AudioDeviceSelector
			jassert ( ads );
			if ( ads )
				ads->setAudioDeviceManager ( adm );
		}

		// Set the HVSC status for the HVSC location component
		setHVSCStatus ( hvscStatus, hvscStatusMessage );

		// Hook the EQ widget up to the FFT measurements stored before the build
		eqCurve = componentutils::findComponent<GUI_EQCurve> ( "audio-device/eq", componentMap );
		if ( eqCurve != nullptr && fftLeft != nullptr && fftRight != nullptr )
			eqCurve->setFFTSources ( *fftLeft, *fftRight );

		// The boot-screen drop-down lists the Basic screens on disk, stored
		// as the file name without extension
		if ( auto combo = componentutils::findComponent<GUI_ComboBox> ( "ui/boot-screen", componentMap ) )
		{
			// The selector factory hides the arrow (footer style), settings show it
			combo->getProperties ().set ( "drawArrow", true );

			for ( const auto& file : datasource::listFiles ( "C64 Screens", false, "*.petmate" ) )
				if ( file.startsWithIgnoreCase ( "Basic" ) )
					bootScreens.add ( file.upToLastOccurrenceOf ( ".", false, false ) );

			std::sort ( bootScreens.begin (), bootScreens.end (), [] ( const juce::String& a, const juce::String& b )
						{	return lime::str::naturalCompare ( a.toRawUTF8 (), b.toRawUTF8 () ) < 0;	} );

			for ( const auto& name : bootScreens )
				combo->addItem ( name, combo->getNumItems () + 1 );

			combo->onChange = [ this, combo ]
			{
				const auto	id = combo->getSelectedId ();
				if ( id <= 0 )
					return;

				juce::SharedResourcePointer<Preferences> ()->set ( "player/boot-screen", bootScreens[ id - 1 ] );

				msg::SettingChanged { "player", "boot-screen" }.send ();
			};
		}

		// The player-screen drop-down (shown when a tune has no artwork):
		// "Random", then the Player screens on disk
		if ( auto combo = componentutils::findComponent<GUI_ComboBox> ( "ui/player-screen", componentMap ) )
		{
			combo->getProperties ().set ( "drawArrow", true );

			playerScreens.add ( "Random" );
			combo->addItem ( juce::SharedResourcePointer<Strings> ()->get ( "settings/player-screen-random" ), 1 );
			combo->addSeparator ();

			for ( const auto& file : datasource::listFiles ( "C64 Screens", false, "*.petmate" ) )
				if ( file.startsWithIgnoreCase ( "Player " ) )
					playerScreens.add ( file.upToLastOccurrenceOf ( ".", false, false ) );

			std::sort ( playerScreens.begin () + 1, playerScreens.end (), [] ( const juce::String& a, const juce::String& b )
						{	return lime::str::naturalCompare ( a.toRawUTF8 (), b.toRawUTF8 () ) < 0;	} );

			for ( auto i = 1; i < playerScreens.size (); ++i )
				combo->addItem ( playerScreens[ i ], i + 1 );

			combo->onChange = [ this, combo ]
			{
				const auto	id = combo->getSelectedId ();
				if ( id <= 0 )
					return;

				juce::SharedResourcePointer<Preferences> ()->set ( "player/player-screen", playerScreens[ id - 1 ] );

				msg::SettingChanged { "player", "player-screen" }.send ();
			};
		}

		// Live example under the export name pattern, colored by which token
		// produced what, red when it can't parse; destination or format picks
		// re-render it
		if ( auto nameEdit = componentutils::findComponent<GUI_SettingsText> ( "export/name-template", componentMap ) )
		{
			const juce::SharedResourcePointer<Strings>	strings;
			nameEdit->setHelpSegments ( colorizeTokenHelp ( strings->get ( "settings/export/name-template-help" ) ) );

			nameEdit->setPreview ( [ this ] ( const juce::String& pattern ) -> std::optional<std::vector<GUI_ColorText::Segment>>
			{
				const juce::SharedResourcePointer<Preferences>	preferences;

				auto	sample = exportname::make ( pattern.toStdString (), "Commando", "Rob Hubbard", "1985 Elite",
													preferences->get<juce::String> ( "player/quality" ).toStdString (), 2, 1, true );

				if ( ! sample )
					return std::nullopt;

				std::ranges::replace ( *sample, '/', char ( juce::File::getSeparatorChar () ) );

				std::vector<GUI_ColorText::Segment>	segments;
				segments.push_back ( { settings->get<juce::String> ( "paths/export" ) + juce::File::getSeparatorString (), UI::colors::textMuted } );

				appendMarkedSegments ( segments, *sample );

				segments.push_back ( { "." + preferences->get<juce::String> ( "export/format" ).toLowerCase (), UI::colors::textMuted } );

				return segments;
			} );

			if ( auto formatChoice = componentutils::findComponent<GUI_SettingsChoice> ( "export/format", componentMap ) )
				formatChoice->onChanged = [ nameEdit ] { nameEdit->refreshPreview (); };

			if ( auto exportLocation = componentutils::findComponent<GUI_SettingsLocation> ( "export/location", componentMap ) )
				exportLocation->onChanged = [ nameEdit ] { nameEdit->refreshPreview (); };
		}

		if ( auto checkToggle = componentutils::findComponent<GUI_SettingsToggle> ( "updates/check", componentMap ) )
			checkToggle->onChanged = [ this ] { updateDisablers (); };

		restorePreferences ();

		// The tuning sliders exist in developer builds only
		const auto sliderConnect = [ this ] ( const juce::String& sldName )
		{
			if ( auto slider = componentutils::findComponent<GUI_CRTSliderLabel> ( sldName, componentMap ) )
				slider->onValueChange = []
				{
					msg::SettingChanged { "fx" }.send ();
				};
		};

		sliderConnect ( "fx/fx-hum-volume" );
		sliderConnect ( "fx/fx-wide-mono-width" );
		sliderConnect ( "fx/fx-splitter-freq" );
		sliderConnect ( "fx/fx-splitter-low-gain" );
		sliderConnect ( "fx/fx-delay-wet" );
		sliderConnect ( "fx/fx-delay-feedback" );
		sliderConnect ( "fx/fx-reverb-wet" );
		sliderConnect ( "fx/fx-noise-volume" );
		sliderConnect ( "fx/fx-noise-color" );
		sliderConnect ( "fx/fx-epic-wide-mono-width" );
		sliderConnect ( "fx/fx-epic-delay-wet" );
		sliderConnect ( "fx/fx-epic-delay-feedback" );
		sliderConnect ( "fx/fx-epic-reverb-wet" );
		sliderConnect ( "fx/fx-mythic-wide-mono-width" );
		sliderConnect ( "fx/fx-mythic-delay-wet" );
		sliderConnect ( "fx/fx-mythic-delay-feedback" );
		sliderConnect ( "fx/fx-mythic-reverb-wet" );
	}
}
//-----------------------------------------------------------------------------

void GUI_Settings::refreshExportPreview ()
{
	if ( componentMap.empty () )
		return;

	if ( auto nameEdit = componentutils::findComponent<GUI_SettingsText> ( "export/name-template", componentMap ) )
		nameEdit->refreshPreview ();
}
//-----------------------------------------------------------------------------

void GUI_Settings::setFFTSources ( const FFTMeasurement& left, const FFTMeasurement& right )
{
	fftLeft = &left;
	fftRight = &right;

	if ( eqCurve != nullptr )
		eqCurve->setFFTSources ( left, right );
}
//-----------------------------------------------------------------------------

void GUI_Settings::spectrumChanged ( const bool stereo )
{
	if ( eqCurve != nullptr )
		eqCurve->spectrumChanged ( stereo );
}
//-----------------------------------------------------------------------------

void GUI_Settings::restorePreferences ()
{
	if ( componentMap.empty () )
		return;

	// Plain combos, restored by stored name; an unknown name selects nothing
	if ( auto combo = componentutils::findComponent<GUI_ComboBox> ( "ui/boot-screen", componentMap ) )
		combo->setSelectedId ( bootScreens.indexOf ( juce::SharedResourcePointer<Preferences> ()->get<juce::String> ( "player/boot-screen" ) ) + 1, juce::dontSendNotification );

	if ( auto combo = componentutils::findComponent<GUI_ComboBox> ( "ui/player-screen", componentMap ) )
		combo->setSelectedId ( playerScreens.indexOf ( juce::SharedResourcePointer<Preferences> ()->get<juce::String> ( "player/player-screen" ) ) + 1, juce::dontSendNotification );

	for ( auto& [ _, comp ] : componentMap )
	{
		if ( auto num = dynamic_cast<GUI_SettingsNumberEdit*> ( comp ) )
			num->restorePreference ();
		else if ( auto	tog = dynamic_cast<GUI_SettingsToggle*> ( comp ) )
			tog->restorePreference ();
		else if ( auto	cho = dynamic_cast<GUI_SettingsChoice*> ( comp ) )
			cho->restorePreference ();
		else if ( auto	txt = dynamic_cast<GUI_SettingsText*> ( comp ) )
			txt->restorePreference ();
		else if ( auto	ads = dynamic_cast<GUI_AudioDeviceSelector*> ( comp ) )
			ads->setCurrentOutputDevice ( settings->get<juce::String> ( "output/device" ) );
		else if ( auto	ts = dynamic_cast<GUI_ThemeSelector*> ( comp ) )
			ts->restorePreference ();
		else if ( auto	ud = dynamic_cast<GUI_SettingsUserData*> ( comp ) )
			ud->refresh ();
	}

	if ( eqCurve != nullptr )
		eqCurve->restorePreferences ();

	updateDisablers ();
}
//-----------------------------------------------------------------------------

void GUI_Settings::updateDisablers ()
{
	// The update frequency only matters while online checks are on
	if ( auto disabler = componentutils::findComponent<GUI_Disabler> ( "updates/disabler", componentMap ) )
		disabler->setEnabled ( juce::SharedResourcePointer<Preferences> ()->get<bool> ( "update/check" ) );
}
//-----------------------------------------------------------------------------

void GUI_Settings::setHVSCStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message )
{
	if ( componentMap.empty () )
	{
		hvscStatus = status;
		hvscStatusMessage = message;
		return;
	}

	if ( const auto sta = componentutils::findComponent<GUI_SettingsLocation> ( "storage/hvsc", componentMap ) )
		sta->updateStatus ( status, message );
}
//-----------------------------------------------------------------------------
