#pragma once

#include <JuceHeader.h>

#include <functional>

//-----------------------------------------------------------------------------

// App-specific context-menu builders: they pull the Playlists/Tags/Strings/
// Icons globals and emit bus messages (Helpers/Messages.h). Generic menu-item
// helpers (newMenuItem etc.) live in UI_Helpers.h.

namespace UI
{
	void menu_ToggleTag ( juce::PopupMenu& m, const juce::StringArray& tunes );

	void menu_AddToPlaylist ( juce::PopupMenu& m, const juce::StringArray& tunes );
	void menu_RemoveFromPlaylist ( juce::PopupMenu& m, const juce::String& plName, const juce::SparseSet<int>& rows );
	void menu_MoveItems ( juce::PopupMenu& m, const juce::String& plName, const juce::SparseSet<int>& rows );

	void menu_GoToFolder ( juce::PopupMenu& m, const juce::String& folder );
	void menu_ExportTrack ( juce::PopupMenu& m, const juce::StringArray& tunes );
	void menu_ExportPlaylist ( juce::PopupMenu& m, const juce::String& plName );

	void menu_DeleteCover ( juce::PopupMenu& m, const juce::String& plName );
	void menu_DeletePlaylist ( juce::PopupMenu& m, const juce::String& plName );

	void menu_ClearOlderThan ( juce::PopupMenu& m, std::function<void ( double days )> clear );
}
//-----------------------------------------------------------------------------
