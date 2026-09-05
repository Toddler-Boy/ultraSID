#pragma once

#include <JuceHeader.h>

#include <optional>
#include <vector>

//-----------------------------------------------------------------------------

// The user folder as portable categories: what each one holds on disk, a zip
// archive of the picked ones, and the import back into a (possibly different)
// user folder. Archive paths are user-root relative, so an archive unpacks
// into any user folder as-is

namespace userdata
{
	enum class Category { playlists, likes, history, tunes, themes, crt, preferences, count };

	// Fixed lowercase token per category, shared by the string keys and the UI ids
	[[ nodiscard ]] juce::String idOf ( Category category );

	// Existing files of a category under root
	[[ nodiscard ]] juce::Array<juce::File> listFiles ( const juce::File& root, Category category );

	// Writes the picked categories into zip; returns the file count, -1 on failure
	[[ nodiscard ]] int exportArchive ( const juce::File& root, const std::vector<Category>& categories, const juce::File& zip );

	// The categories an archive holds; nullopt = not a user-data archive
	[[ nodiscard ]] std::optional<std::vector<Category>> inspectArchive ( const juce::File& zip );

	// merge adds and overwrites by name (likes and history become the union of
	// both sides), replace clears each picked category first
	enum class Mode { merge, replace };

	// Unpacks the picked categories into root; returns the file count, -1 on failure
	[[ nodiscard ]] int importArchive ( const juce::File& root, const juce::File& zip, const std::vector<Category>& categories, Mode mode );

	// Copies the whole folder tree, then verifies the copy file by file
	[[ nodiscard ]] bool copyFolder ( const juce::File& from, const juce::File& to );
}
//-----------------------------------------------------------------------------
