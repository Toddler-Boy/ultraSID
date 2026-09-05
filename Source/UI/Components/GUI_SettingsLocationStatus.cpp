#include "GUI_SettingsLocationStatus.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_SettingsLocationStatus::GUI_SettingsLocationStatus ()
	: juce::Component ( "status" )
{
}
//-----------------------------------------------------------------------------

void GUI_SettingsLocationStatus::lookAndFeelChanged ()
{
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsLocationStatus::paint ( juce::Graphics& g )
{
	g.setColour ( findColour ( UI::colors::statusOk + int ( status ) ) );
	g.setFont ( UI::font ( UI::fonts::settings_location ) );
	g.drawText ( statusMessage, getLocalBounds (), juce::Justification::centredLeft, true );
}
//-----------------------------------------------------------------------------

void GUI_SettingsLocationStatus::setStatus ( const Status _status, const juce::String& _statusMsg )
{
	status = _status;
	statusMessage = _statusMsg;

	setVisible ( statusMessage.isNotEmpty () );
	repaint ();
}
//-----------------------------------------------------------------------------
