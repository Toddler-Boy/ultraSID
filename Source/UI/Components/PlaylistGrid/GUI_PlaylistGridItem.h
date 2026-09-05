#pragma once

#include <JuceHeader.h>

#include "std_lime/lime_string_utils.h"

#include "UI/Components/GUI_CoverDisplay.h"
#include "UI/Components/GUI_PlayButton.h"


class GUI_Pages;
class MipMap;

//-----------------------------------------------------------------------------

class GUI_PlaylistGridItem final
	: public juce::Button
	, public juce::DragAndDropTarget
	, private juce::ChangeListener
	, private juce::Timer
	, public juce::FileDragAndDropTarget
	, public juce::TextDragAndDropTarget
{
public:
	GUI_PlaylistGridItem ( GUI_Pages& pages, const juce::String& name, const bool mini );
	~GUI_PlaylistGridItem () override;

	// juce::Component
	void resized () override;
	void focusGained ( FocusChangeType cause ) override;
	void mouseEnter ( const juce::MouseEvent& e ) override;
	void mouseExit ( const juce::MouseEvent& e ) override;
	void mouseMove ( const juce::MouseEvent& e ) override;
	void lookAndFeelChanged () override;

	// juce::Button
	void paintButton ( juce::Graphics& g, bool isHover, bool isDown ) override;

	void mouseDown ( const juce::MouseEvent& e ) override;
	void mouseUp ( const juce::MouseEvent& e ) override;

	// juce::DragAndDropTarget
	bool isInterestedInDragSource ( const SourceDetails& dragSourceDetails ) override;
	void itemDropped ( const SourceDetails& dragSourceDetails ) override;
	void itemDragEnter ( const SourceDetails& /*dragSourceDetails*/ ) override	{ startHoverAnim (); }
	void itemDragExit ( const SourceDetails& /*dragSourceDetails*/ ) override { stopHoverAnim (); }

	// juce::FileDragAndDropTarget
	bool isInterestedInFileDrag ( const juce::StringArray& files ) override;
	void filesDropped ( const juce::StringArray& files, int x, int y ) override;
	void fileDragEnter ( const juce::StringArray& /*files*/, int /*x*/, int /*y*/ ) override { startHoverAnim (); }
	void fileDragExit ( const juce::StringArray& /*files*/ ) override { stopHoverAnim (); }

	// juce::TextDragAndDropTarget
	bool isInterestedInTextDrag ( const juce::String& text ) override;
	void textDropped ( const juce::String& text, int x, int y ) override;
	void textDragEnter ( const juce::String& /*text*/, int /*x*/, int /*y*/ ) override { startHoverAnim (); }
	void textDragExit ( const juce::String& /*text*/ ) override { stopHoverAnim (); }

	// this
	void setImages ( const std::vector<const Database::entry*>& mips );
	void setImage ( juce::Image& image );
	void setAuthors ( const juce::StringArray& authors );
	void setBasicInfo ( const juce::String& info );

	// A count increase spawns a +N badge over the tile, a decrease -N;
	// the first call is the silent baseline
	void setTuneCount ( const int count );

	// Called with the cause whenever the item gains the keyboard focus
	std::function<void ( FocusChangeType )>	onFocus;

	[[ nodiscard ]] static int compareElements ( GUI_PlaylistGridItem* a, GUI_PlaylistGridItem* b ) {
		return lime::str::naturalCompare ( a->getName ().toRawUTF8 (), b->getName ().toRawUTF8 () );
	}

private:
	GUI_Pages&			pages;
	const bool			mini;

	juce::String		info;
	juce::String		authors;
	int					tuneCount = -1;
	GUI_CoverDisplay	coverDisplay;

	juce::Colour		cardCol;
	juce::Colour		cardTextCol;

	bool	isDragging = false;
	bool	showPlayButton = false;

	GUI_PlayButton				playButton;
	juce::ComponentAnimator&	animator = juce::Desktop::getInstance ().getAnimator ();

	// juce::ChangeListener
	void changeListenerCallback ( juce::ChangeBroadcaster* source ) override;

	float	animCur = 0.0f;
	float	animTarget = 0.0f;
	float	ease = 0.0f;

	// juce::Timer
	void timerCallback () override;

	// this
	void startHoverAnim ( const bool dragging = true );
	void stopHoverAnim ();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_PlaylistGridItem )
};
//-----------------------------------------------------------------------------
