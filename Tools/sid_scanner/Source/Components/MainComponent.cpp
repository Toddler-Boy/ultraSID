#include "MainComponent.h"

#include "sid_scanner.h"

//-----------------------------------------------------------------------------

bool MainComponent::keyPressed ( const juce::KeyPress& key )
{
	if ( key == juce::KeyPress ( juce::KeyPress::F11Key, juce::ModifierKeys::shiftModifier, 0 ) )
	{
		// Toggle log-window, like ultraSID's Shift+F11
		const lime::LoggerOptions opts {
			.name = "sid_scanner",
			.settingsFolder = "sid_scanner",
		};

		auto&	lw = lime::Logger::getInstance ()->getLoggingWindow ( opts );
		lw.setVisible ( ! lw.isVisible () );

		return true;
	}

	return false;
}
//-----------------------------------------------------------------------------

MainComponent::MainComponent ( juce::PropertiesFile* settings, const bool batch )
	: batchMode ( batch )
{
	setWantsKeyboardFocus ( true );

	addAndMakeVisible ( queueView );
	addAndMakeVisible ( activeView );
	queueModel.attachTo ( queueView );
	activeModel.attachTo ( activeView );

	addAndMakeVisible ( toolbar );
	toolbar.getPatternInput ().setHistoryStorage ( settings );
	toolbar.getPatternInput ().onAddPattern = [ this ] ( const juce::String& pattern, const bool force6581, const bool force8580 )
	{
		processingThread.addPattern ( pattern, force6581, force8580 );
	};

	toolbar.onBuildDatabase = [ this ]
	{
		toolbar.setBuildProgress ( 0.0f );
		processingThread.requestDatabaseBuild ();
	};

	addAndMakeVisible ( errorFilterButton );
	errorFilterButton.onClick = [ this ]
	{
		queueModel.setEntryList ( errorFilterButton.getToggleState () ? &failedEntries : nullptr );
		lastQueueSize = -1;	// force an updateContent on the next timer tick
		queueView.updateContent ();
	};

	addAndMakeVisible ( errorCountLabel );
	errorCountLabel.setText ( "Errors: 0", juce::dontSendNotification );

	addAndMakeVisible ( footer );

	setSize ( 800, 600 );

	processingThread.addActionListener ( this );
	processingThread.startThread ();

	startTimerHz ( 10 );
}
//-----------------------------------------------------------------------------

MainComponent::~MainComponent ()
{
	stopTimer ();

	processingThread.stopThread ( -1 );
	processingThread.removeActionListener ( this );
}
//-----------------------------------------------------------------------------

void MainComponent::paint ( juce::Graphics& g )
{
	g.fillAll ( getLookAndFeel ().findColour ( juce::ResizableWindow::backgroundColourId ) );
}
//-----------------------------------------------------------------------------

void MainComponent::resized ()
{
	auto	area = getLocalBounds ();

	toolbar.setBounds ( area.removeFromTop ( 40 ) );
	footer.setBounds ( area.removeFromBottom ( 24 ) );

	area.reduce ( 8, 0 );

	area.removeFromTop ( 8 );

	const auto	leftWidth = ( area.getWidth () - 8 ) / 2;

	// Header strip: error filter toggle and error count above the full queue,
	// empty space above the active view
	auto	headerArea = area.removeFromTop ( 24 );
	auto	leftHeader = headerArea.removeFromLeft ( leftWidth );
	errorFilterButton.setBounds ( leftHeader.removeFromLeft ( 110 ) );
	errorCountLabel.setBounds ( leftHeader );

	area.removeFromTop ( 8 );
	area.removeFromBottom ( 8 );

	queueView.setBounds ( area.removeFromLeft ( leftWidth ) );
	area.removeFromLeft ( 8 );
	activeView.setBounds ( area );
}
//-----------------------------------------------------------------------------

void MainComponent::actionListenerCallback ( const juce::String& message )
{
	const auto	type = message.upToFirstOccurrenceOf ( " ", false, false );
	const auto	text = message.fromFirstOccurrenceOf ( " ", false, false );

	if ( type == "error" )
		handleError ( text );
	else if ( type == "hvsc" )
		handleHVSC ( text );
	else if ( type == "dbbuilt" )
	{
		const auto	ok = text == "ok";

		toolbar.setBuildStatus ( ok
			? "db built " + juce::Time::getCurrentTime ().formatted ( "%H:%M:%S" )
			: "db build FAILED (see log, Shift+F11)" );

		if ( batchMode )
			quitBatch ( ok ? 0 : 20 );
	}
	else
		jassertfalse;	// Unknown message type, see ProcessingThread.h for the list
}
//-----------------------------------------------------------------------------

void MainComponent::handleError ( const juce::String& text )
{
	// Fatal start-up errors and rejected patterns arrive here; per-tune
	// failures live in the queue
	if ( batchMode )
	{
		Z_ERR ( text );
		quitBatch ( 20 );
		return;
	}

	juce::NativeMessageBox::showMessageBoxAsync ( juce::MessageBoxIconType::WarningIcon, ProjectInfo::projectName, text );
}
//-----------------------------------------------------------------------------

void MainComponent::quitBatch ( const int exitCode )
{
	auto*	app = juce::JUCEApplication::getInstance ();

	app->setApplicationReturnValue ( exitCode );
	app->systemRequestedQuit ();
}
//-----------------------------------------------------------------------------

void MainComponent::handleHVSC ( const juce::String& text )
{
	const auto	parts = juce::StringArray::fromTokens ( text, "|", "" );
	if ( parts.size () != 3 )
	{
		jassertfalse;	// Malformed hvsc message
		return;
	}

	if ( auto* window = dynamic_cast<juce::DocumentWindow*> ( getTopLevelComponent () ) )
		window->setName ( juce::String ( ProjectInfo::projectName ) + " - HVSC " + parts[ 1 ] + " - " + parts[ 2 ] + " entries - HVSC: " + parts[ 0 ].replaceCharacter ( '\\', '/' )
						  + " - Data: " + dataRoot ().getFullPathName ().replaceCharacter ( '\\', '/' ) );
}
//-----------------------------------------------------------------------------

void MainComponent::timerCallback ()
{
	// A running database build (manual or the automatic post-scan one) shows
	// its progress in the toolbar; the dbbuilt message swaps back to text
	if ( const auto progress = processingThread.getDbProgress (); progress >= 0.0f )
		toolbar.setBuildProgress ( progress );

	const auto	numEntries = processingThread.getNumQueueEntries ();

	// Rebuild the running and failed snapshots, and gather the footer stats in
	// the same pass
	activeEntries.clear ();
	failedEntries.clear ();
	auto		firstRunningRow = -1;
	auto		remaining = 0;
	uint64_t	totalMs = 0;
	uint64_t	doneMs = 0;
	uint64_t	recentAudioMs = 0;
	uint64_t	recentWallMs = 0;
	uint64_t	sumRenderedMs = 0;

	const auto	nowMs = juce::Time::getMillisecondCounter ();

	// Rendering speed comes from whatever data is current: tunes rendering right
	// now (they publish partial samples every 10s of audio) plus tunes completed
	// within the last minute, so the display is meaningful right from the start
	auto addSpeedSample = [ & ] ( const ProcessingThread::QueueEntry& entry )
	{
		const auto	sample = entry.speedSample.load ();

		recentAudioMs += sample >> 32;
		recentWallMs += sample & 0xffffffffu;
	};

	for ( auto i = 0; i < numEntries; ++i )
	{
		const auto*	entry = processingThread.getQueueEntry ( i );
		if ( entry == nullptr )
			continue;

		totalMs += entry->lengthMS;
		sumRenderedMs += entry->renderedMS.load ();

		switch ( entry->state.load () )
		{
			case ProcessingThread::EntryState::pending:
				++remaining;
				break;

			case ProcessingThread::EntryState::running:
				++remaining;
				doneMs += std::min ( entry->renderedMS.load (), entry->lengthMS );
				addSpeedSample ( *entry );

				activeEntries.push_back ( entry );
				if ( firstRunningRow < 0 )
					firstRunningRow = i;
				break;

			case ProcessingThread::EntryState::done:
				doneMs += entry->lengthMS;

				if ( nowMs - entry->completedAtMs.load () < 60000 )
					addSpeedSample ( *entry );
				break;

			case ProcessingThread::EntryState::failed:
				doneMs += entry->lengthMS;
				failedEntries.push_back ( entry );
				break;
		}
	}

	errorCountLabel.setText ( "Errors: " + juce::String ( static_cast<int> ( failedEntries.size () ) ), juce::dontSendNotification );

	const auto	errorsOnly = errorFilterButton.getToggleState ();

	const auto	fullViewRows = errorsOnly ? static_cast<int> ( failedEntries.size () ) : numEntries;
	if ( fullViewRows != lastQueueSize )
	{
		lastQueueSize = fullViewRows;
		queueView.updateContent ();
	}

	if ( static_cast<int> ( activeEntries.size () ) != lastActiveSize )
	{
		lastActiveSize = static_cast<int> ( activeEntries.size () );
		activeView.updateContent ();
	}

	// Keep the full queue view from going stale: whenever the top-most running entry
	// changes, scroll it to the top of the view (leaving the user's scroll position
	// alone in between). Not while filtered to errors, those rows don't move
	if ( ! errorsOnly && firstRunningRow >= 0 && firstRunningRow != lastFirstRunningRow )
	{
		lastFirstRunningRow = firstRunningRow;

		if ( auto* viewport = queueView.getViewport () )
			viewport->setViewPosition ( viewport->getViewPositionX (), firstRunningRow * queueView.getRowHeight () );
	}

	// New work arriving after the queue completely finished resets the
	// measurement to the fresh scan
	if ( remaining > 0 && queueFinished )
	{
		processingStartMs = 0.0;
		elapsedMs = 0;
		renderedBaselineMs = sumRenderedMs;
	}
	queueFinished = remaining == 0 && numEntries > 0;

	// Update the footer: elapsed time starts with the first queue entry and
	// freezes once nothing is left to do
	if ( numEntries > 0 )
	{
		if ( processingStartMs == 0.0 )
			processingStartMs = juce::Time::getMillisecondCounterHiRes ();

		if ( remaining > 0 )
			elapsedMs = int64_t ( juce::Time::getMillisecondCounterHiRes () - processingStartMs );
	}

	FooterComponent::Stats	stats;
	stats.remaining = remaining;
	stats.total = numEntries;
	stats.fractionDone = totalMs ? double ( doneMs ) / double ( totalMs ) : 0.0;
	stats.elapsedMs = elapsedMs;
	if ( recentWallMs > 0 )
		lastSpeedRatio = double ( recentAudioMs ) / double ( recentWallMs );

	stats.speedRatio = lastSpeedRatio;

	// The total is the plain ratio of rendered audio to elapsed wall time; both
	// freeze once the queue drains, so the final number stays. A forced
	// re-add can remove rendered work from the queue, hence the clamp
	if ( elapsedMs > 0 && sumRenderedMs > renderedBaselineMs )
		stats.totalSpeedRatio = double ( sumRenderedMs - renderedBaselineMs ) / double ( elapsedMs );
	if ( remaining == 0 && numEntries > 0 )
		stats.totalTimeMs = elapsedMs;
	else if ( stats.fractionDone > 0.001 )
		stats.totalTimeMs = int64_t ( elapsedMs / stats.fractionDone );

	footer.setStats ( stats );

	// Repaint the visible rows so running progress bars and state changes show up
	queueView.repaint ();
	activeView.repaint ();
}
//-----------------------------------------------------------------------------
