#include "GUI_VoicePitch.h"

//-----------------------------------------------------------------------------

GUI_VoicePitch::GUI_VoicePitch ()
{
	setName ( "pitch" );
}
//-----------------------------------------------------------------------------

void GUI_VoicePitch::addPitch ( const float pitch )
{
	constexpr auto	highestNote = 1.0f / 127.0f;

	const auto	data = std::clamp ( pitch * highestNote, 0.0f, 1.0f );

	addDatapoint ( data );
}
//-----------------------------------------------------------------------------
