#pragma once

#include <JuceHeader.h>

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Helpers/FileUtils.h"

//-----------------------------------------------------------------------------

class Likes final
{
public:
	Likes () = default;

	void setRoot ( const juce::File& file )
	{
		root = file;
		load ();
	}

	void toggle ( const std::string_view likeView, const int subtune )
	{
		const std::string	like ( likeView );

		Z_INFO ( "Toggling like: " << like << " subtune: " << subtune );

		auto	it = likes.find ( like );
		if ( it != likes.end () )
		{
			auto	numIt = std::find ( it->second.begin (), it->second.end (), subtune );
			if ( numIt == it->second.end () )
			{
				it->second.emplace_back ( subtune );
			}
			else
			{
				likes[ like ].erase ( numIt );
				if ( likes[ like ].empty () )
					likes.erase ( like );
			}
		}
		else
		{
			likes.emplace ( like, std::vector<int> { subtune } );
		}

		save ();
	}

	[[ nodiscard ]] bool isLiked ( const std::string_view like, const int subtune ) const
	{
		auto	it = likes.find ( like );
		if ( it == likes.end () )
			return false;

		return std::find ( it->second.begin (), it->second.end (), subtune ) != it->second.end ();
	}

	[[ nodiscard ]] bool isLiked ( const std::string_view like ) const
	{
		return likes.contains ( like );
	}

	// Liked subtunes over all tunes
	[[ nodiscard ]] int numEntries () const
	{
		auto	n = 0;

		for ( const auto& [ _, subTunes ] : likes )
			n += int ( subTunes.size () );

		return n;
	}

private:
	void load ()
	{
		likes.clear ();

		if ( root == juce::File () )
			return;

		auto	likeFile = root.getChildFile ( "likes.txt" );
		if ( ! likeFile.existsAsFile () )
			return;

		auto	lines = juce::StringArray ();
		likeFile.readLines ( lines );

		for ( const auto& line : lines )
		{
			// Tune names may contain commas, so the subtune follows the last one
			const auto	comma = line.lastIndexOfChar ( ',' );
			if ( comma < 0 )
				continue;

			likes[ line.substring ( 0, comma ).toStdString () ].emplace_back ( line.substring ( comma + 1 ).getIntValue () );
		}
	}

	void save ()
	{
		if ( root == juce::File () )
			return;

		root.createDirectory ();

		auto	likeFile = root.getChildFile ( "likes.txt" );

		auto	lines = juce::StringArray ();
		for ( const auto& [ like, subTunes ] : likes )
			for ( auto subTune : subTunes )
				lines.add ( like + "," + std::to_string ( subTune ) );

		lines.sortNatural ();

		fileutils::replaceFile ( likeFile, lines.joinIntoString ( "\r\n" ) );
	}

	juce::File	root;

	std::unordered_map<std::string, std::vector<int>, lime::str::TransparentHash, std::equal_to<>>	likes;
};
//-----------------------------------------------------------------------------
