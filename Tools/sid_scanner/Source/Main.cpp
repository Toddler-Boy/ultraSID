#include <JuceHeader.h>

#include "Components/MainComponent.h"
#include "CustomLookAndFeel.h"
#include "DatabaseBuilder.h"

//-----------------------------------------------------------------------------

class UltraSIDToolApplication final : public juce::JUCEApplication
{
public:
	UltraSIDToolApplication ()
	{
		juce::Logger::setCurrentLogger ( lime::Logger::getInstance () );
	}

	~UltraSIDToolApplication () override
	{
		juce::Logger::setCurrentLogger ( nullptr );
	}

	const juce::String getApplicationName () override		{ return ProjectInfo::projectName; }
	const juce::String getApplicationVersion () override	{ return ProjectInfo::versionString; }
	bool moreThanOneInstanceAllowed () override				{ return false; }

	void initialise ( const juce::String& commandLine ) override
	{
		// --build-db: no GUI, join the scanner's databases with the HVSC
		// documents into ultraSID's Data/ultraSID.db, then quit
		const auto	args = juce::StringArray::fromTokens ( commandLine, true );

		if ( args.contains ( "--build-db" ) )
		{
			setApplicationReturnValue ( buildDatabase () );
			quit ();
			return;
		}

		// --batch: scan the command-line patterns, build the database and quit
		// (exit code 20 on any error), for scripted runs
		const auto	batch = args.contains ( "--batch" );

		// Install the app-wide look-and-feel before any component is created, so
		// every colour lookup already sees the final values
		juce::LookAndFeel::setDefaultLookAndFeel ( &lookAndFeel );

		// Window state lives in the per-user app-data folder as XML; saving is
		// debounced: every change restarts the 2s countdown, so the file is
		// only written once the window has been left alone for that long
		juce::PropertiesFile::Options	options;
		options.applicationName = ProjectInfo::projectName;
		options.filenameSuffix = ".settings";
		options.osxLibrarySubFolder = "Application Support";
		options.storageFormat = juce::PropertiesFile::storeAsXML;
		options.millisecondsBeforeSaving = 2000;

		appProperties.setStorageParameters ( options );

		mainWindow = std::make_unique<MainWindow> ( ProjectInfo::projectName, appProperties.getUserSettings (), batch );
	}

	void shutdown () override
	{
		lime::Logger::getInstance ()->closeLoggingWindow ();

		if ( mainWindow != nullptr )
			mainWindow->flushWindowState ();

		mainWindow = nullptr;

		juce::LookAndFeel::setDefaultLookAndFeel ( nullptr );

		appProperties.saveIfNeeded ();
		appProperties.closeFiles ();
	}

	void systemRequestedQuit () override
	{
		quit ();
	}

	void anotherInstanceStarted ( const juce::String& /*commandLine*/ ) override {}

	//-----------------------------------------------------------------------------

	class MainWindow final : public juce::DocumentWindow
	{
	public:
		MainWindow ( const juce::String& name, juce::PropertiesFile* settingsIn, const bool batch )
			: DocumentWindow ( name,
							   juce::Desktop::getInstance ().getDefaultLookAndFeel ().findColour ( juce::ResizableWindow::backgroundColourId ),
							   DocumentWindow::allButtons ),
			  settings ( settingsIn )
		{
			setUsingNativeTitleBar ( true );
			setContentOwned ( new MainComponent ( settingsIn, batch ), false );
			setResizable ( true, false );
			setResizeLimits ( 800, 400, 8192, 8192 );

			const auto	windowState = settings != nullptr ? settings->getValue ( "windowState" ) : juce::String ();
			if ( windowState.isNotEmpty () )
				restoreWindowStateFromString ( windowState );
			else
				centreWithSize ( getWidth (), getHeight () );

			setVisible ( true );
		}

		void closeButtonPressed () override
		{
			juce::JUCEApplication::getInstance ()->systemRequestedQuit ();
		}

		// Records the final window state and detaches from the settings, so the
		// bogus bounds changes during window teardown can't overwrite it
		void flushWindowState ()
		{
			saveWindowState ();
			settings = nullptr;
		}

		void moved () override
		{
			DocumentWindow::moved ();
			saveWindowState ();
		}

		void resized () override
		{
			DocumentWindow::resized ();
			saveWindowState ();
		}

	private:
		void saveWindowState ()
		{
			// Never save while the window isn't on screen: getWindowStateAsString()
			// only tracks a showing window, so construction- and teardown-time
			// moved()/resized() calls would clobber the stored state with defaults
			if ( settings != nullptr && isShowing () )
				settings->setValue ( "windowState", getWindowStateAsString () );
		}

		juce::PropertiesFile*	settings = nullptr;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( MainWindow )
	};

private:
	CustomLookAndFeel			lookAndFeel;
	std::unique_ptr<MainWindow>	mainWindow;
	juce::ApplicationProperties	appProperties;
};
//-----------------------------------------------------------------------------

START_JUCE_APPLICATION ( UltraSIDToolApplication )
//-----------------------------------------------------------------------------
