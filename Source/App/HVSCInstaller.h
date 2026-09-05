#pragma once

#include <JuceHeader.h>

#include <functional>

#include "App/InstallState.h"
#include "Config/Settings.h"

//-----------------------------------------------------------------------------

// Downloads and installs/updates the High Voltage SID Collection: pulls the
// full archive or the update chain up to the targeted release from the CDN,
// extracts it on a background thread, and applies update scripts. All state
// is published through the shared InstallState; the UI observes via the
// callbacks below (all invoked on the message thread) and the message bus.

class HVSCInstaller final : private juce::Thread
{
public:
	HVSCInstaller ();
	~HVSCInstaller () override;

	// The HVSC root folder, keep in sync with the settings path
	void setRoot ( const juce::File& root )		{	hvscRoot = root;	}

	// Blocks until any running extraction has stopped
	void stop ()	{	stopThread ( -1 );	}

	// Async flows (start from the message thread)
	void downloadUpdate ();
	void downloadFull ();
	void cancelFullInstall ();
	void cancelUpdate ();

	// Human-readable install status for the location widgets
	enum class Status : int8_t
	{
		ok,
		warning,
		error
	};

	void reportHVSCStatus ();
	void reportDatabaseStatus ();

	// UI hooks, all invoked on the message thread
	std::function<void ( Status, const juce::String& )>	onStatus;
	std::function<void ()>				onInstallFinished;	// reload roots & databases
	std::function<void ( bool full )>	onCanceling;		// show cancelation UI
	std::function<void ( bool full )>	onCanceled;			// back to the start page

private:
	// The worker thread outlives nothing, but its posted results can outlive us, so
	// everything it hands back to the message thread goes through here
	void postAsync ( std::function<void ( HVSCInstaller& )> fn );

	// juce::Thread, archive extraction & update scripts
	void run () override;

	// Downloads the update archives for `version` up to targetVersion in
	// sequence, then starts the worker that applies them all in one pass
	void downloadNextUpdate ( const int version );

	void extractFull ( const juce::MemoryBlock& data );

	// Formats the versioned download url; empty on a bad template, with the
	// failure logged and reported
	[[ nodiscard ]] std::string formatURL ( const std::string& tmpl, int version, const char* what, const char* statusOnError );

	// The shared download wiring; onData runs only on a good response
	void startDownload ( const std::string& url, std::function<void ( const juce::MemoryBlock& )> onData );

	void report ( const Status status, const juce::String& message );

	// The slow part of a full-install cancel: stops extraction and deletes
	// the partial tree
	class Canceler final : public juce::Thread
	{
	public:
		Canceler ( HVSCInstaller& i ) : juce::Thread ( "HVSC cancel" ), installer ( i ) {}
		~Canceler () override	{	stopThread ( -1 );	}

	private:
		void run () override	{	installer.cancelWork ();	}

		HVSCInstaller&	installer;
	};

	void cancelWork ();

	juce::SharedResourcePointer<InstallState>	installState;
	juce::SharedResourcePointer<Settings>		settings;

	juce::File	hvscRoot;

	gin::DownloadManager	downloader { 5 * 1000, 5 * 1000 };
	juce::MemoryBlock		downloadedData;

	// The pending update chain (versionInstalled+1 ... targetVersion) and
	// its position in the shared download progress bar
	std::vector<juce::MemoryBlock>	downloadedUpdates;
	int								chainIndex = 0;
	int								chainCount = 1;

	enum class task : int8_t
	{
		none,
		update,
		full,
	};

	task	currentTask = task::none;

	std::atomic<bool>	installCanceled = false;

	// Last member: joined first, while everything it uses lives
	Canceler	canceler { *this };

	JUCE_DECLARE_WEAK_REFERENCEABLE ( HVSCInstaller )
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( HVSCInstaller )
};
//-----------------------------------------------------------------------------
