#include "GUI_Chip.h"

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Audio/sid-constants.h"

//-----------------------------------------------------------------------------

GUI_Chip::GUI_Chip ()
{
	addAndMakeVisible ( background );
	addChildComponent ( portrait );
	addAndMakeVisible ( logo );
	addAndMakeVisible ( profile );
	addAndMakeVisible ( chipState );
	addChildComponent ( digiDisplay );

	background.addIndentation ( chipState.getChildComponent ( 0 ) );				// First voice
	background.addIndentation ( chipState.getChildComponent ( SID::numVoices ) );	// Filter
	background.addIndentation ( &digiDisplay );										// Digi display
}
//-----------------------------------------------------------------------------

// Each chip lays out its own children, so any number of chips works without
// the sidebar layout having to know them by name
void GUI_Chip::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/components/chip.json" } );
}
//-----------------------------------------------------------------------------

void GUI_Chip::setModel ( const std::string& _model )
{
	logo.setModel ( _model );

	is6581 = _model == "6581";
}
//-----------------------------------------------------------------------------

void GUI_Chip::setDigiVisible ( const bool shouldBeVisible )
{
	digiDisplay.setVisible ( shouldBeVisible );
	background.setDigiVisible ( shouldBeVisible );
}
//-----------------------------------------------------------------------------

void GUI_Chip::setProfile ( const std::string& chipProfile, const std::string& chipProfileBitmap, const bool goldenBorder )
{
	profileName = chipProfile;
	isApproved = goldenBorder;

	const auto	is8580 = ! is6581;

	profile.setDotColor ( is8580 || ! chipProfile.empty () );

	if ( chipProfile.starts_with ( "emu-" ) )
		portrait.setBitmap ( chipProfile, false );
	else
		portrait.setBitmap ( is8580 ? "" : chipProfileBitmap, is6581 && goldenBorder );
}
//-----------------------------------------------------------------------------

juce::String GUI_Chip::getTooltip ()
{
	const juce::SharedResourcePointer<Strings>	strings;

	// Generic tooltip
	if ( profileName.isEmpty () )
		return strings->get ( is6581 ? "chip/sid6581_profile_tip" : "chip/sid8580_profile_tip" );

	if ( isApproved )
		return strings->get ( "chip/approved" ).replace ( "{}", profileName );

	return profileName.replace ( "emu-", "" );
}
//-----------------------------------------------------------------------------
