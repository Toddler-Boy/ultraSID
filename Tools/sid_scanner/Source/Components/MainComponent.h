#pragma once

#include <JuceHeader.h>

#include "ProcessingThread.h"
#include "QueueModel.h"

#include "FooterComponent.h"
#include "ToolbarComponent.h"

//-----------------------------------------------------------------------------

class MainComponent final : public juce::Component,
							public juce::ActionListener,
							private juce::Timer
{
public:
	// batch: the app quits after the database build (exit 20 on any error)
	MainComponent ( juce::PropertiesFile* settings, bool batch );
	~MainComponent () override;

	void paint ( juce::Graphics& g ) override;
	void resized () override;
	bool keyPressed ( const juce::KeyPress& key ) override;

private:
	void actionListenerCallback ( const juce::String& message ) override;

	void handleError ( const juce::String& text );
	void handleHVSC ( const juce::String& text );

	void quitBatch ( int exitCode );

	const bool			batchMode;

	// Polls the queue so progress bars, states and the active view stay current
	void timerCallback () override;

	ProcessingThread	processingThread;

	std::vector<const ProcessingThread::QueueEntry*>	activeEntries;
	std::vector<const ProcessingThread::QueueEntry*>	failedEntries;

	QueueModel			queueModel { processingThread };
	QueueModel			activeModel { processingThread, &activeEntries };

	juce::TableListBox	queueView;
	juce::TableListBox	activeView;
	ToolbarComponent	toolbar;
	juce::ToggleButton	errorFilterButton { "Errors only" };
	juce::Label			errorCountLabel;
	FooterComponent		footer;
	juce::TooltipWindow	tooltipWindow { this };

	int					lastQueueSize = 0;
	int					lastActiveSize = 0;
	int					lastFirstRunningRow = -1;

	double				processingStartMs = 0.0;
	int64_t				elapsedMs = 0;
	double				lastSpeedRatio = -1.0;

	// New work after a completely finished queue starts a fresh measurement:
	// the rendered sum above the baseline over the restarted elapsed clock
	uint64_t			renderedBaselineMs = 0;
	bool				queueFinished = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( MainComponent )
};
//-----------------------------------------------------------------------------
