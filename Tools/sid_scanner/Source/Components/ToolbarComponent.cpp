#include "ToolbarComponent.h"

#include "CustomLookAndFeel.h"

//-----------------------------------------------------------------------------

ToolbarComponent::ToolbarComponent ()
{
	addAndMakeVisible ( patternInput );

	addAndMakeVisible ( buildStatus );
	buildStatus.setJustificationType ( juce::Justification::centredRight );

	addChildComponent ( progressBar );

	addAndMakeVisible ( buildButton );
	buildButton.setTooltip ( "Joins the measurements with the HVSC documents into ultraSID.db. A finished scan builds automatically" );
	buildButton.onClick = [ this ]
	{
		if ( onBuildDatabase )
			onBuildDatabase ();
	};
}
//-----------------------------------------------------------------------------

PatternInputComponent& ToolbarComponent::getPatternInput ()
{
	return patternInput;
}
//-----------------------------------------------------------------------------

void ToolbarComponent::setBuildStatus ( const juce::String& text )
{
	progressBar.setVisible ( false );
	buildStatus.setVisible ( true );
	buildStatus.setText ( text, juce::dontSendNotification );
}
//-----------------------------------------------------------------------------

void ToolbarComponent::setBuildProgress ( const float fraction )
{
	buildProgress = fraction;
	buildStatus.setVisible ( false );
	progressBar.setVisible ( true );
}
//-----------------------------------------------------------------------------

void ToolbarComponent::paint ( juce::Graphics& g )
{
	g.fillAll ( getShade ( 0.0f ) );
}
//-----------------------------------------------------------------------------

void ToolbarComponent::resized ()
{
	auto	area = getLocalBounds ().reduced ( 8 );

	buildButton.setBounds ( area.removeFromRight ( 80 ) );
	area.removeFromRight ( 6 );
	const auto	statusArea = area.removeFromRight ( 160 );
	buildStatus.setBounds ( statusArea );
	progressBar.setBounds ( statusArea );
	area.removeFromRight ( 8 );

	patternInput.setBounds ( area );
}
//-----------------------------------------------------------------------------
