#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/UI/Components/GUI_ListBoxMouseMoveHover.h"
#include "ultra-shared/UI/Components/GUI_ViewportSmoothScroll.h"

#include "Resources/STIL_Lookup.h"

//----------------------------------------------------------------------------------

class GUI_STIL_ListView final : public juce::TableListBox, public juce::TableListBoxModel, public juce::ChangeListener
{
public:
	enum columnId : int8_t
	{
		animation = 1,	// juce::TableHeaderComponent forbids a columnId of 0
		tuneNo,
		name,
		author,
		length,
		liked,
	};

	GUI_STIL_ListView ();
	~GUI_STIL_ListView () override;

	// juce::ListBox
	bool keyPressed ( const juce::KeyPress& key ) override;

	// juce::TableListBoxModel
	int getNumRows () override;
	void cellClicked ( int row, int column, const juce::MouseEvent& e ) override;
	void cellDoubleClicked ( int row, int column, const juce::MouseEvent& e ) override;
	void returnKeyPressed ( int lastRowSelected ) override;
 	void paintRowBackground ( juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected ) override;
 	void paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected ) override;
	juce::var getDragSourceDescription ( const juce::SparseSet<int>& rowsToDescribe ) override;
	juce::String getCellTooltip ( int rowNumber, int columnId ) override;

	// this
	void layout ();
	void setTune ( const juce::String& name, const int mainTuneNo );
	void setBlocks ( const GUI_STIL_blocks& blocks );
	void setTunePlaying ( const int tune );
	void setDefaultTune ( const std::string& title, const int tune );
	void setTuneLength ( const int tune, int lengthMS );
	void timerUpdate ( const float secondsPassed );

	[[ nodiscard ]] int getMaximumHeight ();

	[[ nodiscard ]] bool hasStingers () const;
	[[ nodiscard ]] bool hasSongs () const;
	[[ nodiscard ]] bool onlyHasStingers () const;
	[[ nodiscard ]] bool onlyHasSongs () const;

	// juce::ChangeListener
	void changeListenerCallback ( juce::ChangeBroadcaster* source ) override;
	int		hoverPosition = -1;
	GUI_ListBoxMouseMoveHover	hover;
	GUI_ViewportSmoothScroll	smoothScroll;

private:
	juce::SharedResourcePointer<Icons>	icons;

	int			tunePlaying = -1;
	float		animState = -1.0f;
	juce::Font	font { juce::FontOptions {} };

	juce::String	tuneName;
	int				mainTuneNo = -1;

	struct tuneEntry
	{
		int				no;
		juce::String	tuneName;
		juce::String	authorName;
		int				lengthMS = 0;
		juce::String	timeStr;
		juce::String	categoryStr;
		bool			songFlag = false;
	};

	std::vector<tuneEntry>			sourceData;
	std::vector<const tuneEntry*>	rowData;
	std::vector<int>				rowMap;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_ListView )
};
//----------------------------------------------------------------------------------
