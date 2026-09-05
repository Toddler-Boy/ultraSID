#include <JuceHeader.h>

#include "ultra-shared/App/AppUpdater.h"
#include "ultra-shared/Helpers/PlatformHelper.h"
#include "ultra-shared/Resources/Strings.h"

#include "App/TuneExporter.h"
#include "Config/Settings.h"
#include "Helpers/Messages.h"
#include "UI/GUI_AppLookAndFeel.h"
#include "UI/GUI_ultraSID.h"

//-----------------------------------------------------------------------------

class ultraSIDApp : public juce::JUCEApplication
{
public:
	ultraSIDApp ()
	{
		juce::Logger::setCurrentLogger ( lime::Logger::getInstance () );
	}

	~ultraSIDApp () override
	{
		juce::Logger::setCurrentLogger ( nullptr );
	}

	const juce::String getApplicationName () override { return ProjectInfo::projectName; }
	const juce::String getApplicationVersion () override { return ProjectInfo::versionString; }
	bool moreThanOneInstanceAllowed () override { return false; }

	//-----------------------------------------------------------------------------

	void initialise ( const juce::String& /*commandLine*/ ) override
	{
		// The running version shows in the sidebar's version pill instead
		auto	title = getApplicationName ();

		#if ULTRA_DEVELOPMENT
			title += " - development";
		#elif JUCE_DEBUG
			title += " - debug";
		#endif

		mainWindow.reset ( new MainWindow ( title, laf ) );

		// The Authenticode digest seals code and appended pak in one hash, so
		// this doubles as a data-integrity check. Off the message thread, the
		// verify hashes the whole exe. Hard-coded text: a corrupted file must
		// not source its error message from its own (corrupted) data
		juce::Thread::launch ( juce::Thread::Priority::low, [ this ]
		{
			if ( verifyExecutableSignature () != SignatureState::corrupted )
				return;

			Z_ERR ( "Executable signature check failed, the file is corrupted" );

			juce::MessageManager::callAsync ( [ this ]
			{
				mainWindow->setVisible ( false );

				juce::NativeMessageBox::showMessageBoxAsync ( juce::MessageBoxIconType::WarningIcon,
					"ultraSID is damaged",
					"The ultraSID program file is corrupted. Please download and install it again.",
					nullptr,
					juce::ModalCallbackFunction::create ( [] ( int ) { juce::JUCEApplication::quit (); } ) );
			} );
		} );
	}
	//-----------------------------------------------------------------------------

	void shutdown () override
	{
		// Export workers message UI objects the window owns, stop them first
		tuneExporter->stopThreads ();

		mainWindow = nullptr;
	}
	//-----------------------------------------------------------------------------

	void systemRequestedQuit () override
	{
		if ( tuneExporter->getNumWorkEntries () == 0 )
			return quit ();

		juce::NativeMessageBox::showYesNoBox ( juce::MessageBoxIconType::WarningIcon,
			strings->get ( "quit/title" ), strings->get ( "quit/message" ),
			nullptr,
			juce::ModalCallbackFunction::create ( [] ( int r )
			{
				if ( r == 1 )
					quit ();
			} )
		);
	}
	//-----------------------------------------------------------------------------

	void anotherInstanceStarted ( const juce::String& /*commandLine*/ ) override
	{
	}
	//-----------------------------------------------------------------------------

	class MainWindow : public juce::DocumentWindow
	{
	public:
		MainWindow ( juce::String name, juce::LookAndFeel& laf )
			: juce::DocumentWindow ( name, juce::Colours::black, juce::DocumentWindow::allButtons )
		{
			juce::LookAndFeel::setDefaultLookAndFeel ( &laf );

			//layout.setConstant ( "mac", 1 );
			//layout.setConstant ( "win", 0 );
			//layout.setConstant ( "linux", 0 );

			// Set up window
			setUsingNativeTitleBar ( true );

			auto	ultra = new GUI_ultraSID;

			setContentOwned ( ultra, false );

			setResizeLimits ( 1'280, 700, 100'000, 100'000 );

			setResizable ( true, false );
			setWantsKeyboardFocus ( false );

			// Restore state
			{
				const auto	pos = settings->get<juce::String> ( "ui/window-position" );
				restoreWindowStateFromString ( pos );

				// First start, center window and set size
				if ( pos.isEmpty () )
				{
					const auto&	displays = juce::Desktop::getInstance ().getDisplays ();
					const auto*	display = displays.getDisplayForRect ( getScreenBounds () );

					if ( display )
					{
						const auto	b = display->userBounds.toNearestIntEdges ();
						centreWithSize ( std::clamp ( b.getWidth () - 100, 890, 1280 ),
										 std::clamp ( b.getHeight () - 25, 700, 100'000 ) );
					}
				}
				else
				{
					// If window is out of screen to the top or left, center it
					if ( const auto rect = getBounds (); rect.getX () < 0 || rect.getY () < 0 )
						centreWithSize ( rect.getWidth (), rect.getHeight () );
				}
			}

			msg::RestoreState {}.send ();

			setVisible ( true );
			bringWindowToForeground ( getWindowHandle () );
		}

		~MainWindow () override
		{
			lime::Logger::getInstance ()->closeLoggingWindow ();
		}

		#if JUCE_WINDOWS || JUCE_MAC
			void parentHierarchyChanged () override
			{
				juce::DocumentWindow::parentHierarchyChanged ();
				setBorderColor ();
			}
		#endif

		void colourChanged () override
		{
			juce::DocumentWindow::colourChanged ();
			setBorderColor ();
		}

		void closeButtonPressed () override
		{
			auto	content = dynamic_cast<GUI_ultraSID*> ( getContentComponent () );
			content->saveState ();

			// Exports and installs never coexist, so stopping the installer
			// before the quit dialog is safe
			content->prepareToQuit ();
			JUCEApplication::getInstance ()->systemRequestedQuit ();
		}

		void resized () override
		{
			juce::DocumentWindow::resized ();

			if ( ! isVisible () )
				return;

			saveState ();
		}

		void moved () override
		{
			juce::DocumentWindow::moved ();

			// Pre-visible moves are construction artifacts (macOS pushes the
			// window below the menu bar); saving one would clobber the stored
			// position before the constructor restores it
			if ( ! isVisible () )
				return;

			if ( auto cc = getContentComponent () )
			{
				cc->moved ();
				saveState ();
			}
		}

	private:
		void saveState ()
		{
			settings->set ( "ui/window-position", getWindowStateAsString () );
		}

		void setBorderColor ()
		{
			if ( auto peer = getPeer () )
				setWindowProperties ( peer->getNativeHandle (), getBackgroundColour ().getARGB () );
		}

		juce::SharedResourcePointer<Settings>	settings;

		juce::Colour	titleColor { 0xFF10141C };

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( MainWindow )
	};

private:
	std::unique_ptr<MainWindow> mainWindow;
	GUI_AppLookAndFeel			laf;

	juce::SharedResourcePointer<TuneExporter>	tuneExporter;
	juce::SharedResourcePointer<Strings>		strings;
};
//-----------------------------------------------------------------------------

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ( "-Wmissing-prototypes" )
JUCE_CREATE_APPLICATION_DEFINE ( ultraSIDApp )

// An installed update relaunches once JUCE has released the instance lock
JUCE_MAIN_FUNCTION
{
	juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;

	const auto	result = juce::JUCEApplicationBase::main ( JUCE_MAIN_FUNCTION_ARGS );

	AppUpdater::relaunchIfInstalled ();

	return result;
}
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
