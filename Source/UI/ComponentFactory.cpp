#include "ComponentFactory.h"

#include "ultra-shared/UI/Components/GUI_AudioDeviceSelector.h"
#include "ultra-shared/UI/Components/GUI_CRTSliderLabel.h"
#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_SettingsAction.h"
#include "ultra-shared/UI/Components/GUI_SettingsChoice.h"
#include "ultra-shared/UI/Components/GUI_SettingsNumberEdit.h"
#include "ultra-shared/UI/Components/GUI_SettingsToggle.h"
#include "ultra-shared/UI/SharedComponentFactory.h"

#include "Audio/FXTuning.h"
#include "UI/Components/GUI_EQCurve.h"
#include "UI/Components/GUI_SettingsInstall.h"
#include "UI/Components/GUI_SettingsLocation.h"
#include "UI/Components/GUI_SettingsText.h"
#include "UI/Components/GUI_SettingsUserData.h"

//-----------------------------------------------------------------------------

std::pair<juce::Component*, bool> componentFactory ( const juce::String& typeName )
{
	auto	typeParts = juce::StringArray::fromTokens ( typeName, "(,)", "" );
	typeParts.trim ();
	const auto	compType = typeParts[ 0 ].toLowerCase ();
	typeParts.remove ( 0 );
	typeParts.removeEmptyStrings ();

	//
	// Settings toggle (with description text)
	//
	if ( compType == "set-toggle" )
		return { new GUI_SettingsToggle ( typeParts[ 0 ], typeParts[ 1 ] ), false };

	//
	// Settings number edit (with description text)
	//
	if ( compType == "set-number" )
		return { new GUI_SettingsNumberEdit ( typeParts[ 0 ], typeParts[ 1 ], typeParts[ 2 ].equalsIgnoreCase ( "float" ) ), false };

	//
	// Settings choice (with description text), the options follow section and key
	//
	if ( compType == "set-choice" )
	{
		auto	options = typeParts;
		options.removeRange ( 0, 2 );

		return { new GUI_SettingsChoice ( typeParts[ 0 ], typeParts[ 1 ], options ), false };
	}

	//
	// Settings text edit (with description text); the third argument names
	// its layout json
	//
	if ( compType == "set-text" )
		return { new GUI_SettingsText ( typeParts[ 0 ], typeParts[ 1 ], typeParts[ 2 ] ), false };

	//
	// Settings location; an optional "move" argument adds the move button
	//
	if ( compType == "set-location" )
		return { new GUI_SettingsLocation ( typeParts[ 0 ], typeParts.size () > 1 && typeParts[ 1 ].equalsIgnoreCase ( "move" ) ), false };

	//
	// User data export/import block
	//
	if ( compType == "set-user-data" )
		return { new GUI_SettingsUserData, false };

	//
	// Start menu shortcut and programs-folder move (Windows)
	//
	if ( compType == "set-install" )
		return { new GUI_SettingsInstall, false };

	//
	// Title, help and a button sending a message verb: set-action(name, verb)
	//
	if ( compType == "set-action" )
		return { new GUI_SettingsAction ( typeParts[ 0 ], typeParts[ 1 ] ), false };

	//
	// Audio-enhancement tuning slider (developer builds), bound to the
	// process-wide FXTuning value of that name
	//
	if ( compType == "fx-slider" )
	{
		auto*	value = fxTuning ().slot ( typeParts[ 0 ] );

		if ( ! value )
		{
			Z_ERR ( "Unknown FX tuning slider: " << typeParts[ 0 ] );

			static float	orphan = 0.0f;
			value = &orphan;
		}

		return { new GUI_CRTSliderLabel ( "crt/settings/fx/" + typeParts[ 0 ], *value, typeParts[ 1 ].equalsIgnoreCase ( "true" ) ), false };
	}

	//
	// Audio device selector
	//
	if ( compType == "audio-device" )
		return { new GUI_AudioDeviceSelector, false };

	//
	// Header1
	//
	if ( compType == "header1" )
	{
		jassert ( typeParts.size () == 1 );

		auto	header = new GUI_DynamicLabel ( "settings/header/" + typeParts[ 0 ], UI::fonts::page_title );
		header->setJustification ( juce::Justification::topLeft );

		return { header, false };
	}

	//
	// Header2
	//
	if ( compType == "header2" )
		return { new GUI_DynamicLabel ( "settings/header/" + typeParts[ 0 ], UI::fonts::settings_section ), false };

	//
	// Settings label
	//
	if ( compType == "set-label" )
		return { new GUI_DynamicLabel ( "settings/" + typeParts[ 0 ], UI::fonts::settings_label ), false };

	//
	// Settings entry name and muted help line, for rows composed in the page json
	//
	if ( compType == "set-entry" )
		return { new GUI_DynamicLabel ( "settings/" + typeParts[ 0 ], UI::fonts::settings_entry ), false };

	if ( compType == "set-help" )
		return { new GUI_DynamicLabel ( "settings/" + typeParts[ 0 ], UI::fonts::settings_help, UI::colors::textMuted ), false };

	//
	// The user EQ curve
	//
	if ( compType == "eq-curve" )
		return { new GUI_EQCurve, false };

	//
	// The generic and CRT-settings types
	//
	if ( auto shared = sharedComponentFactory ( compType, typeParts ); shared.first != nullptr )
		return shared;

	// Unknown component type
	Z_ERR ( "Unknown component type: " << compType );
	jassertfalse;
	return { nullptr, false };
}
//-----------------------------------------------------------------------------
