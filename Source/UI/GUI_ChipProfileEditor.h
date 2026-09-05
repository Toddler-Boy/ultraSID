#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_ComboBox.h"
#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_Line.h"
#include "ultra-shared/UI/Components/GUI_Slider.h"
#include "ultra-shared/UI/Components/GUI_Toggle.h"

#include "Audio/SIDPlayer.h"
#include "UI/Components/GUI_TextButton.h"

//-----------------------------------------------------------------------------

// The hidden chip-profile editor (Ctrl+Shift+F9, for sid-authors): its sliders
// feed the live emulation while the render thread is throttled to stay just
// ahead of the playhead, so profile values can be tuned by ear in real-time

class GUI_ChipProfileEditor final : public juce::DocumentWindow
{
public:
	using ChipSettings = SIDPlayer::ChipSettings;

	GUI_ChipProfileEditor ( SIDPlayer& player, const juce::File& userCsvFile, std::function<void ()> onClosed );

	// juce::DocumentWindow
	void closeButtonPressed () override	{	if ( onClosed ) onClosed ();	}

	// this
	// Re-populate from a freshly loaded tune's profile and push it live; also
	// drops the loop region, its times belong to the previous tune
	void refresh ( const ChipSettings& s );

	// Loop region, polled once per v-blank by the main GUI
	[[ nodiscard ]] bool isLoopSet () const				{	return loopEndMS > loopStartMS;	}
	[[ nodiscard ]] uint32_t getLoopStartMS () const	{	return loopStartMS;				}
	[[ nodiscard ]] uint32_t getLoopEndMS () const		{	return loopEndMS;				}

private:
	// A label-plus-slider line; the label text comes from the strings file and
	// the layout id is the key's last segment
	struct SliderRow final : juce::Component
	{
		SliderRow ( const juce::String& stringKey, double min, double max, double increment, double defaultValue );

		// juce::Component
		void resized () override;

		gin::LayoutSupport	layout { *this };

		GUI_DynamicLabel	label;
		GUI_Slider			slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
	};

	// this
	void pushFromControls ();
	[[ nodiscard ]] ChipSettings settingsFromControls () const;
	[[ nodiscard ]] juce::String buildCsvRow () const;
	void saveToUserProfile ();
	void saveToFactoryProfile ();
	bool upsertCsvRow ( const juce::File& csv, const juce::String& name, const juce::String& headerIfEmpty );
	void removeUserProfileRow ( const juce::String& name );
	void updateTitle ();
	void updateLoopLabel ();

	SIDPlayer&	player;
	juce::File	userCsv;

	std::function<void ()>	onClosed;

	// The tune's real profile; keeps what the controls don't edit (approved
	// status and the exceptions cell) intact for the CSV output
	ChipSettings	current;

	uint32_t	loopStartMS = 0;
	uint32_t	loopEndMS = 0;

	juce::SharedResourcePointer<Strings>	strings;

	//
	// Content
	//
	class Panel final : public juce::Component
	{
	public:
		explicit Panel ( GUI_ChipProfileEditor& owner );

		// juce::Component
		void resized () override;

		gin::LayoutSupport	layout { *this };

		GUI_ChipProfileEditor&	owner;

		GUI_DynamicLabel	nameLabel	{ "chip-editor/name", 13.0f, 500 };
		GUI_DynamicLabel	folderLabel	{ "chip-editor/folder", 13.0f, 500 };
		GUI_Label			nameEdit { "", 13.0f, 500, UI::colors::text };
		GUI_Label			folderEdit { "", 13.0f, 500, UI::colors::text };

		SliderRow	flt0Dac	{ "chip-editor/flt0dac",	 0.0, 1.0,	0.05, 0.4 };
		SliderRow	fltGain	{ "chip-editor/fltgain",	 0.5, 1.5,	0.01, 0.92 };
		SliderRow	fltSat	{ "chip-editor/fltsat",		 0.0, 1.0,	0.05, 1.0 };
		SliderRow	resonance { "chip-editor/resonance", 0.0, 1.0,	0.05, 1.0 };
		SliderRow	waveDC	{ "chip-editor/wavedc",		 0.0, 2.0,	0.05, 0.5 };
		SliderRow	extInDC	{ "chip-editor/extdc",		 0.0, 2.0,	0.05, 1.0 };
		SliderRow	bias	{ "chip-editor/bias",		-1.0, 1.0,	0.05, 0.0 };
		SliderRow	leakage	{ "chip-editor/leakage",	 1.0, 20.0,	1.0, 1.0 };

		GUI_DynamicLabel	capLabel		{ "chip-editor/fltcap", 13.0f, 500 };
		GUI_Toggle			capOld			{ "cap" };

		GUI_DynamicLabel	cwsLabel		{ "chip-editor/cws", 13.0f, 500 };
		GUI_ComboBox		cwsLevel		{ "cws" };
		GUI_DynamicLabel	cwsUltraLabel	{ "chip-editor/cws_ultra", 13.0f, 500 };
		GUI_Toggle			cwsUltra		{ "cws-ultra" };

		GUI_DynamicLabel	loopLabel	{ "chip-editor/loop", 13.0f, 500 };
		GUI_Label			loopInfo	{ "", 13.0f, 500, UI::colors::textMuted };
		GUI_TextButton		loopSetStart	{ "loop-start",	"chip-editor/set_start" };
		GUI_TextButton		loopSetEnd		{ "loop-end",	"chip-editor/set_end" };
		GUI_TextButton		loopClear		{ "loop-clear",	"chip-editor/clear" };

		GUI_TextButton		saveUser	{ "save-user",		"chip-editor/save_user" };
		GUI_TextButton		saveFactory	{ "save-factory",	"chip-editor/save_factory" };

		GUI_Line	line1 { "line1" };
		GUI_Line	line2 { "line2" };
		GUI_Line	line3 { "line3" };
		GUI_Line	line4 { "line4" };
		GUI_Line	line5 { "line5" };
	};

	Panel	panel { *this };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ChipProfileEditor )
};
//-----------------------------------------------------------------------------
