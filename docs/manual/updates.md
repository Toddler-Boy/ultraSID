---
title: Updates
---

# Updates

ultraSID updates itself. The version pill in the sidebar tells you where it stands, and a click on it does whatever is next.

## How it works

On start, or as often as you set under Settings, App Updates, ultraSID fetches a small file from ultrasid.com that names the current version, where to get it, and its checksum. If the version is newer, the pill turns into an offer. Click it, the build downloads in the background, its checksum is verified, and the pill asks you to restart. ultraSID replaces itself and relaunches.

- **Windows**: the `.exe` is swapped in place. The old one is kept until the next start, then removed.
- **macOS**: the app bundle's contents are swapped, so Dock and Finder aliases keep working.
- **Linux**: the AppImage is swapped. A bare binary only checks and tells you.

A download that fails the checksum is thrown away, and the pill says so. Nothing is ever installed unverified.

## What is sent

Nothing. The check is a plain request for a static file on the website, the same request a browser makes. No identifier, no usage data, no telemetry. The only other network traffic is the HVSC download and update, from ultraSID's own server, and whatever you drop onto the window yourself.

Turn "Check for updates online" off and ultraSID never talks to the network on its own.

## Damaged files

On Windows, ultraSID checks its own signature on every start, offline, in the background. All of the app's data, the screens, shaders, artwork and databases, travels inside the executable and is covered by that signature. A half-finished download or a file a virus scanner has mangled is caught right there, with a message asking you to download again, instead of failing in strange ways later. On macOS the system does the same check before launch.

## What ultraSID deliberately does not do

- **No installer.** ultraSID is one file. On Windows, Settings, Start Menu, can put it in Programs and give it a Start menu entry, if you want it to look installed.
- **No beta channel.** There is one version, and it is the current one.
