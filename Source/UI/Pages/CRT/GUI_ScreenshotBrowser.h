#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_ViewportSmoothScroll.h"

#include "App/ScreenshotLookup.h"
#include "App/ThumbnailCache.h"

//-----------------------------------------------------------------------------

// The screenshot browser beside the CRT: a path bar over one folder of the
// merged tree; a click shows a picture, a double-click enters a folder

class GUI_ScreenshotBrowser final
	: public juce::Component
	, public juce::ListBoxModel
	, public juce::FileDragAndDropTarget
{
public:
	GUI_ScreenshotBrowser ();
	~GUI_ScreenshotBrowser () override;

	// juce::Component
	void paint ( juce::Graphics& g ) override;
	void resized () override;
	void lookAndFeelChanged () override;

	// juce::ListBoxModel
	int getNumRows () override;
	void paintListBoxItem ( int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected ) override;
	void listBoxItemDoubleClicked ( int row, const juce::MouseEvent& e ) override;
	void returnKeyPressed ( int row ) override;
	void selectedRowsChanged ( int lastRowSelected ) override;
	juce::String getNameForRow ( int rowNumber ) override;

	// juce::FileDragAndDropTarget
	bool isInterestedInFileDrag ( const juce::StringArray& files ) override;
	void filesDropped ( const juce::StringArray& files, int x, int y ) override;

	// this
	void navigateTo ( const juce::String& folder );
	[[ nodiscard ]] const juce::String& getFolder () const	{	return folder;	}

	// The tree changed: re-list the folder, keeping the selection by name
	void refresh ();

	// Highlight the picture the CRT shows, when it is in this folder
	void selectPicture ( const juce::String& artName );

	std::function<void ( const juce::String& artName )>						onPick;
	std::function<void ( const juce::StringArray& files, const juce::String& folder )>	onDropFiles;

	// Thumbnails render in the CRT's TV standard
	bool	isNTSC = false;

private:
	// The folder as clickable crumbs, root first
	class PathBar final : public juce::Component
	{
	public:
		void paint ( juce::Graphics& g ) override;
		void mouseDown ( const juce::MouseEvent& e ) override;
		void mouseMove ( const juce::MouseEvent& e ) override;
		void mouseExit ( const juce::MouseEvent& e ) override;

		void setPath ( const juce::String& rootName, const juce::String& folder );

		std::function<void ( const juce::String& folder )>	onNavigate;

	private:
		juce::StringArray	crumbs;		// [0] = root name
		juce::String		folder;
		std::vector<juce::Rectangle<float>>	crumbBounds;
		int		hoverCrumb = -1;

		[[ nodiscard ]] int crumbAt ( const juce::Point<float> p ) const;
		[[ nodiscard ]] juce::String folderOfCrumb ( const int index ) const;
	};

	// Backspace goes up a folder; the rest is the list's own key handling
	class List final : public juce::ListBox
	{
	public:
		bool keyPressed ( const juce::KeyPress& key ) override;
		std::function<bool ( const juce::KeyPress& )>	extraKey;
	};

	[[ nodiscard ]] bool isFolderRow ( const int row ) const	{	return row < int ( entries.folders.size () );	}
	[[ nodiscard ]] int rowOfPicture ( const juce::String& artName ) const;
	void openRow ( const int row );

	// Select without picking: the CRT already shows the picture
	void selectQuietly ( const int row );

	// Icons recolored to the theme's text color on demand
	[[ nodiscard ]] juce::Drawable* icon ( const juce::String& key );

	PathBar		pathBar;
	List		list;
	GUI_ViewportSmoothScroll	smoothScroll { list };

	juce::String				folder;
	ScreenshotLookup::listing	entries;
	juce::StringArray			colorNames;		// VIC-II palette, for the border hint

	std::map<juce::String, std::unique_ptr<juce::Drawable>>	iconCache;
	juce::Colour	iconColor = juce::Colours::black;

	juce::SharedResourcePointer<ScreenshotLookup>	lookup;
	juce::SharedResourcePointer<ThumbnailCache>		thumbnailCache;
	juce::SharedResourcePointer<Strings>			strings;
	juce::SharedResourcePointer<Icons>				icons;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ScreenshotBrowser )
};
//-----------------------------------------------------------------------------
