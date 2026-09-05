#include "std_lime/lime_string_utils.h"

#include "Config/FilePaths.h"
#include "Database/STIL_Quotes.h"
#include "Resources/STIL_Lookup.h"

#include "GUI_ultraSID.h"


void GUI_ultraSID::preProcessSTIL ( const juce::String& filename, const unsigned int mainSong )
{
	// Only HVSC keys are stripped: STIL paths cover the HVSC alone, and a user
	// tune must never alias one
	const auto	stilName = filename.startsWith ( "$HVSC$/" ) ? filename.substring ( int ( filepaths::hvscMarker.size () ) ) : filename;
	const auto	folderName = stilName.upToLastOccurrenceOf ( "/", true, false );

	auto getStilBlocks = [ this, &folderName ] ( const juce::String& name, const int tuneNo = 0 )
	{
		GUI_STIL_blocks	stilBlocks;

		if ( auto block = hvscDatabase->getSTILEntry ( name.toStdString (), tuneNo ); block.has_value () )
		{
			for ( const auto& [ bName, bValue ] : *block )
			{
				const auto	jname = juce::String ( bName );
				const auto	jvalue = juce::String ( bValue ).trim ();

				if ( jname == "COMMENT" )
				{
					// Detect quotes in comments, and separate them out as new strings
					auto    splitStr = stilq::splitCommentQuotes ( jvalue.toStdString () );

					for ( auto& ent : splitStr )
					{
						auto	jStr = juce::String ( ent.text );

						// [. "] -> insert double return at space
						jStr = jStr.replace ( ". \"", ".\n\n\"" );

						// [" "] -> insert double return at space
						jStr = jStr.replace ( "\" \"", "\"\n\n\"" );

						// [\n ] -> remove space after returns
						jStr = jStr.replace ( "\n ", "\n" );

						jStr = jStr.trim ();
						ent.text = jStr.toStdString ();
					}

					for ( auto& ent : splitStr )
					{
                        if ( ent.text.empty () )
                            continue;

                        // Comment from '/MUSICIANS/G/Galway_Martin/Rambo_First_Blood_Part_II.sid' about Morse-code being broken
						if ( ent.type == "COMMENT" && ent.text.starts_with ( "-...|.|" ) && ent.text.ends_with ( "(Steve Wahid)" ) )
						{
							// One line per Morse phrase: split after each ')' that closes a
							// parenthesised translation. A translation ends in a letter, so a
							// letter before ')' distinguishes it from the "(*)" marker that also
							// appears mid-code.
							// blank-line paragraphs would tear the phrase list apart
							const auto      morse = juce::String ( ent.text ).replace ( "\n\n", "\n" ).toStdString ();

							juce::String    curStr = "";
							size_t          start = 0;

							auto flush = [ & ] ( size_t end )
							{
								if ( auto line = juce::String ( morse.substr ( start, end - start ) ).trim (); line.isNotEmpty () )
									curStr += line + "\n";
								start = end;
							};

							for ( size_t p = 0; p < morse.size (); ++p )
								if ( morse[ p ] == ')' && p > start && morse[ p - 1 ] >= 'a' && morse[ p - 1 ] <= 'z' )
									flush ( p + 1 );

							flush ( morse.size () );   // trailing remainder, if any

							curStr = curStr.replaceCharacters ( ".-|", "‧⁃ " );

							stilBlocks.push_back ( { "MONO", curStr.trimEnd (), "" } );
						}
                        else if ( ent.type == "QUOTE" )
                        {
                            const juce::SharedResourcePointer<STILLookup>	lookup;

                            auto	stilEntry = lookup->findBestEntry ( folderName.toStdString (), ent.speaker );

                            stilBlocks.push_back ( { "QUOTE", juce::String ( ent.text ), juce::String ( stilEntry.name ) } );
                        }
						else if ( ent.type == "COMMENT" )
							stilBlocks.push_back ( { "COMMENT", juce::String ( ent.text ), "" } );
					}
				}
				else
				{
					stilBlocks.push_back ( { jname, jvalue, "" } );
				}
			}
		}

		return stilBlocks;
	};

	// Folder/Author block
	auto	fileBlocks = getStilBlocks ( folderName );

	// Add file block
	auto	tuneBlocks = getStilBlocks ( stilName, 0 );

	// Concatenate all file and tune blocks
	for ( auto tuneIndex = 1u; tuneIndex <= player.getNumberOfSongs (); ++tuneIndex )
	{
		fileBlocks.push_back ( { "TUNE", juce::String ( tuneIndex ), "" } );

		if ( tuneIndex == mainSong )
			fileBlocks.splice ( fileBlocks.end (), tuneBlocks );

		auto	subTuneBlocks = getStilBlocks ( stilName, tuneIndex );
		fileBlocks.splice ( fileBlocks.end (), subTuneBlocks );
	}

	mainScreen.sidebarRight.setSTIL_blocks ( folderName, fileBlocks );
	mainScreen.sidebarRight.setTune ( filename, mainSong );
}
//-----------------------------------------------------------------------------
