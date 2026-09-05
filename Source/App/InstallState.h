#pragma once

#include <JuceHeader.h>

#include <array>
#include <fmt/format.h>

#include "ultra-shared/Helpers/TextUtils.h"

//-----------------------------------------------------------------------------

class InstallState final
{
public:
	InstallState () = default;

	struct _server
	{
		bool			online = false;
		std::string		response;

		void reset ()
		{
			online = false;
			response.clear ();
		}

		void setState ( const int httpResponse )
		{
			online = httpResponse >= 200 && httpResponse < 500;

			if ( httpResponse == 0 )
				response = "Offline";
			else if ( httpResponse >= 200 && httpResponse < 300 )
				response.clear ();
			else
				response = "HTTP/" + std::to_string ( httpResponse );
		}
	};

	struct _hvsc
	{
		// The major app version IS the HVSC release the shipped tune database
		// was built for (CMake checks the database header against it)
		static constexpr int	targetVersion = ProjectInfo::versionNumber >> 16;
		static constexpr int	earliestUpdatable = 68;		// oldest install the update chain can patch, older ones reinstall in full

		int				versionInstalled = -1;			// <= 0 not installed, otherwise the version-number
		std::string		status;
		int				downloadId = 0;

		void reset ()
		{
			status.clear ();
		}

		[[ nodiscard ]] bool needsUpdate () const { return targetVersion > versionInstalled; }
		[[ nodiscard ]] bool needsFullInstall () const { return versionInstalled < 0 || versionInstalled < earliestUpdatable; }
	};

	struct _database
	{
		// Database info
		int				versionInstalled = -1;		// -1 = not loaded yet, 0 = unreadable, otherwise the version-number
		std::string		status;

		void reset ()
		{
			status.clear ();
		}
	};

	struct _progress
	{
		// A worker holds a reference to its stage for the whole run, so the storage is
		// fixed and only ever zeroed, never reallocated under them
		static constexpr int	maxStages = 4;

		juce::StringArray	descriptions;			// status message to show in the UI
		std::array<std::atomic<float>, maxStages>	current = {};
		std::atomic<int>	state = 0;				// Written by the install thread, read by the UI
		std::atomic<int>	maxFiles = 0;
		std::atomic<int>	currentFiles = 0;

		void reset ( const juce::StringArray& _descriptions )
		{
			jassert ( _descriptions.size () <= maxStages );

			descriptions = _descriptions;

			for ( auto& a : current )
				a.store ( 0.0f );

			state = 0;

			maxFiles = 0;
			currentFiles = 0;
		}

		void setState ( const int index )
		{
			state = std::clamp ( index, 0, int ( descriptions.size () ) - 1 );
		}

		std::atomic<float>& operator[] ( const int index )
		{
			jassert ( index >= 0 && index < descriptions.size () );

			// descriptions is empty until the first reset (), and a caller that ignored
			// the assert there could have left it longer than the array
			const auto	last = std::clamp ( descriptions.size () - 1, 0, maxStages - 1 );

			return current[ size_t ( std::clamp ( index, 0, last ) ) ];
		}

		[[ nodiscard ]] juce::String getDescription () const
		{
			if ( descriptions.isEmpty () )
				return {};

			return descriptions[ state ];
		}

		[[ nodiscard ]] juce::String getProgressText () const
		{
			if ( maxFiles == 0 )
				return {};

			if ( ! state )
			{
				// Show in megabytes for downloads
				constexpr auto	megaByte = 1000.0 * 1000.0;
				return fmt::format ( "{:.1f}MB / {:.1f}MB", currentFiles.load () / megaByte, maxFiles.load () / megaByte );
			}

			// Show number of files to extract or lines to process for database updates
			return textutils::getHumanNumber ( currentFiles.load () ) + " / " + textutils::getHumanNumber ( maxFiles.load () );
		}

		[[ nodiscard ]] float getProgress () const
		{
			const auto	stages = std::clamp ( descriptions.size (), 0, maxStages );
			if ( ! stages )
				return 0.0f;

			return std::accumulate ( current.begin (), current.begin () + stages, 0.0f ) / float ( stages );
		}
	};

	std::string		appVersion;

	_server			server;
	_hvsc			hvsc;
	_database		database;
	_progress		progress;

private:
};
//-----------------------------------------------------------------------------
