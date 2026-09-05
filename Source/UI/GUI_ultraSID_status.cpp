#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

void GUI_ultraSID::setHVSCStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message )
{
	mainScreen.pages.setHVSCStatus ( status, message );
	onboardingScreen.setHVSCStatus ( status, message );
	updateHVSCScreen.setHVSCStatus ( status, message );
}
//-----------------------------------------------------------------------------

// The status texts are computed by the installer and arrive back through
// its onStatus hook, which fans them out via setHVSCStatus above

void GUI_ultraSID::checkHVSCStatus ()
{
	hvscInstaller.reportHVSCStatus ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::checkDatabaseStatus ()
{
	hvscInstaller.reportDatabaseStatus ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::resetInstallScreens ()
{
	// Back to the start pages, where the status widgets show what went wrong
	if ( onboardingScreen.isUpdating () )
		onboardingScreen.startOver ();

	if ( updateHVSCScreen.isUpdating () )
		updateHVSCScreen.startOver ();
}
//-----------------------------------------------------------------------------
