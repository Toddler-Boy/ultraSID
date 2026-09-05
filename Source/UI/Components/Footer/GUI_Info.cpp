#include <JuceHeader.h>

#include "GUI_Info.h"

#include "libSidplayEZ/src/EZ/SidTuneInfoEZ.h"

#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

GUI_Info::GUI_Info ()
{
	setName ( "info" );

	title.setName ( "title" );
	author.setName ( "author" );
	released.setName ( "released" );

	thumbnail.onClick = []
	{
		msg::ShowPage { "crt" }.send ();
	};

	addAndMakeVisible ( thumbnail );

	addAndMakeVisible ( title );
	addAndMakeVisible ( author );
	addAndMakeVisible ( released );
}
//-----------------------------------------------------------------------------

void GUI_Info::setStrings ( const SidTuneInfoEZ& src )
{
	title.setText ( src.title );
	author.setText ( src.author );
	released.setText ( src.released );
}
//-----------------------------------------------------------------------------
