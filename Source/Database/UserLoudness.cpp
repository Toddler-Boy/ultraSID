#include <fmt/format.h>

#include "UserLoudness.h"

#include "Config/FilePaths.h"

//-----------------------------------------------------------------------------

UserLoudness::UserLoudness ()
{
	file = filepaths::getUserLoudnessPath ();
	if ( file == juce::File () || ! file.existsAsFile () )
		return;

	auto	lines = juce::StringArray::fromLines ( file.loadFileAsString () );
	lines.trim ();
	lines.removeEmptyStrings ();

	for ( const auto& line : lines )
	{
		const auto	eq = line.indexOfChar ( '=' );
		if ( eq <= 0 )
			continue;

		auto&	vec = db[ line.substring ( 0, eq ).toStdString () ];
		vec.clear ();

		auto	tokens = juce::StringArray::fromTokens ( line.substring ( eq + 1 ), " ", "" );
		tokens.removeEmptyStrings ();

		for ( const auto& token : tokens )
		{
			const auto	slash = token.indexOfChar ( '/' );
			vec.emplace_back ( token.getFloatValue (),
							   slash >= 0 ? token.substring ( slash + 1 ).getFloatValue () : -96.0f );
		}
	}
}
//-----------------------------------------------------------------------------

std::string UserLoudness::keyFor ( const std::string& tuneKey )
{
	auto	name = std::string ( filepaths::stripLocationMarker ( std::string_view ( tuneKey ) ) );

	if ( name.starts_with ( '/' ) )
		name.erase ( 0, 1 );
	if ( name.ends_with ( ".sid" ) )
		name.erase ( name.size () - 4 );

	return name;
}
//-----------------------------------------------------------------------------

std::pair<float, float> UserLoudness::get ( const std::string& key, const unsigned int songNo ) const
{
	if ( const auto it = db.find ( key ); songNo && it != db.end () && songNo <= it->second.size () )
		return it->second[ songNo - 1 ];

	return { -96.0f, -96.0f };
}
//-----------------------------------------------------------------------------

void UserLoudness::store ( const std::string& key, const unsigned int songNo, const float lufs, const float midLufs )
{
	if ( file == juce::File () || ! songNo )
		return;

	auto&	vec = db[ key ];
	if ( vec.size () < songNo )
		vec.resize ( songNo, { -96.0f, -96.0f } );

	// The scanner's convention: unmeasurable loudness stores the -14 default
	vec[ songNo - 1 ] = { lufs > -96.0f ? lufs : -14.0f,
						  midLufs > -96.0f ? midLufs : -96.0f };

	save ();
}
//-----------------------------------------------------------------------------

void UserLoudness::save () const
{
	juce::String	out;

	for ( const auto& [ name, vec ] : db )
	{
		out << name << "=";

		for ( auto cnt = 0u; const auto& [ lufs, mid ] : vec )
			out << fmt::format ( "{:.1f}/{:.1f}", lufs, mid ).c_str ()
				<< ( ++cnt == vec.size () ? "\r\n" : " " );
	}

	file.replaceWithText ( out );
}
//-----------------------------------------------------------------------------
