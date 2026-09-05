#pragma once

#include <JuceHeader.h>

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/FileUtils.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

class Tags final
{
public:
	struct tagEntry
	{
		std::string	filename;
		std::string	name;
		int			colorId;

		// Folder tags hold folder keys (trailing slash) instead of tune keys;
		// toggling ignores the individual selection and flips the whole folder
		bool		folder = false;
	};

	Tags () = default;
	//-----------------------------------------------------------------------------

	void reload ()
	{
		tags.clear ();

		for ( const auto& tag : getTagEntries () )
		{
			auto&	vec = tags[ tag.name ];

			const auto	content = datasource::loadText ( "Tags/" + tag.filename + ".txt" );

			auto	lines = juce::StringArray::fromLines ( content );

			lines.removeEmptyStrings ();
			lines.trim ();
			lines.sortNatural ();

			for ( const auto& line : lines )
				vec.emplace ( line.toStdString () );
		}
	}
	//-----------------------------------------------------------------------------

	[[ nodiscard ]] const std::vector<tagEntry>& getTagEntries () const
	{
		return tagEntries;
	}
	//-----------------------------------------------------------------------------

	[[ nodiscard ]] bool isTagged ( const std::string& tagName, const std::string_view tune ) const
	{
		auto	it = tags.find ( tagName );

		if ( it == tags.end () )
			return false;

		if ( it->second.contains ( tune ) )
			return true;

		// A tune inherits its folder's tag
		if ( const auto pos = tune.rfind ( '/' ); pos != std::string_view::npos )
			return it->second.contains ( tune.substr ( 0, pos + 1 ) );

		return false;
	}
	//-----------------------------------------------------------------------------

	void toggleTags ( const std::string& tagName, const juce::StringArray& tunes )
	{
		// Resolved before anything is touched: an unknown name has no file to save to,
		// and tags[] would otherwise leave a phantom entry behind
		const auto	tag = std::ranges::find_if ( tagEntries, [ &tagName ] ( const auto& t ) { return t.name == tagName; } );
		if ( tag == tagEntries.end () )
		{
			Z_ERR ( "Unknown tag: " << tagName );
			return;
		}

		auto&	vec = tags[ tagName ];

		// Two tunes from the same folder must not toggle it twice
		auto	keys = juce::StringArray {};
		for ( const auto& entry : tunes )
			keys.addIfNotAlreadyThere ( tag->folder ? entry.upToLastOccurrenceOf ( "/", true, false ) : entry );

		for ( const auto& entry : keys )
		{
			const auto	tune = entry.toStdString ();

			if ( auto it = std::ranges::find ( vec, tune ); it != vec.end () )
				vec.erase ( it );
			else
				vec.emplace ( tune );
		}

		// Save tag file (tagging is developer curation, so the naked file exists)
		{
			auto	content = juce::StringArray {};
			for ( const auto& entry : vec )
				content.add ( entry );

			content.removeEmptyStrings ();
			content.trim ();
			content.sortNatural ();

			const auto	tagFile = datasource::getDevFile ( "Tags/" + tag->filename + ".txt" );

			if ( tagFile == juce::File () )
				Z_ERR ( "Could not save tag " << tag->filename );
			else
				fileutils::replaceFile ( tagFile, content.joinIntoString ( "\r\n" ) );
		}
	}
	//-----------------------------------------------------------------------------

private:
	const std::vector<tagEntry>		tagEntries =
	{
		{ "Pioneers",	"search/tag/pioneers",	UI::colors::tagPioneers,	true },
		{ "Winners",	"search/tag/winners",	UI::colors::tagWinners },
		{ "Gems",		"search/tag/gems",		UI::colors::tagGems },
	};

	std::map<std::string, std::unordered_set<std::string, lime::str::TransparentHash, std::equal_to<>>>	tags;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( Tags )
};
//-----------------------------------------------------------------------------
