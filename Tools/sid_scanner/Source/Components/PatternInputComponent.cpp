#include "PatternInputComponent.h"

//-----------------------------------------------------------------------------

PatternInputComponent::PatternInputComponent ()
{
	addAndMakeVisible ( patternBox );
	patternBox.setEditableText ( true );
	patternBox.setTextWhenNothingSelected ( "regex pattern to add to the queue" );

	addAndMakeVisible ( force6581Button );
	force6581Button.setTooltip ( "Re-measure matching tunes with a 6581 even when the database already has them" );

	addAndMakeVisible ( force8580Button );
	force8580Button.setTooltip ( "Re-measure matching pure 8580 tunes even when the database already has them (their emulation effectively never changes)" );

	addAndMakeVisible ( addButton );
	addButton.onClick = [ this ] { addPattern (); };
}
//-----------------------------------------------------------------------------

void PatternInputComponent::resized ()
{
	auto	area = getLocalBounds ();

	addButton.setBounds ( area.removeFromRight ( 100 ) );
	area.removeFromRight ( 8 );
	force8580Button.setBounds ( area.removeFromRight ( 90 ) );
	force6581Button.setBounds ( area.removeFromRight ( 90 ) );
	area.removeFromRight ( 8 );
	patternBox.setBounds ( area );
}
//-----------------------------------------------------------------------------

void PatternInputComponent::setHistoryStorage ( juce::PropertiesFile* settingsIn )
{
	settings = settingsIn;

	history.clear ();
	if ( settings != nullptr )
	{
		history.addLines ( settings->getValue ( "patternHistory" ) );
		history.removeEmptyStrings ();
	}

	refreshHistoryItems ();
}
//-----------------------------------------------------------------------------

void PatternInputComponent::refreshHistoryItems ()
{
	const auto	text = patternBox.getText ();

	patternBox.clear ( juce::dontSendNotification );
	patternBox.addItemList ( history, 1 );
	patternBox.setText ( text, juce::dontSendNotification );
}
//-----------------------------------------------------------------------------

void PatternInputComponent::addPattern ()
{
	const auto	pattern = patternBox.getText ().trim ();
	if ( pattern.isEmpty () || onAddPattern == nullptr )
		return;

	onAddPattern ( pattern, force6581Button.getToggleState (), force8580Button.getToggleState () );

	// Most recent first, no duplicates, capped
	history.removeString ( pattern );
	history.insert ( 0, pattern );
	while ( history.size () > maxHistoryEntries )
		history.remove ( history.size () - 1 );

	if ( settings != nullptr )
		settings->setValue ( "patternHistory", history.joinIntoString ( "\n" ) );

	patternBox.setText ( {}, juce::dontSendNotification );
	refreshHistoryItems ();
}
//-----------------------------------------------------------------------------
