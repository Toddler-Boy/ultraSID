---
title: STIL and tune info
---

# STIL and tune info

The right sidebar shows what is known about the playing tune: the STIL notes, the list of subtunes, and what the chip is doing.

## STIL

The SID Tune Information List is the HVSC's liner notes: who wrote the tune, what game or demo it was for, which song it covers, and the composer's own comments. ultraSID shows the notes for the folder, the file and the current subtune, and follows along when the subtune changes.

Quotes inside the notes are shown as quotes, with the speaker's name resolved from the HVSC folder name to the person, so "Galway" reads as Martin Galway. Known bugs from the HVSC bug list are shown too.

ultraSID adds its own small addendum: names for about a hundred tunes' worth of subtunes that the STIL leaves anonymous, mostly stingers, sound effects and speech samples. It is how "FX 7" becomes "Game over".

The "Notes" toggle hides the text when you only want the list.

## The subtune list

Every subtune in the file, with its number, name, artist where the file has several, length and a heart to like it on its own. Double-click plays it once, without touching the playlist.

Each row carries an icon for its kind: song, stinger, sound effect or speech. The kind comes from the length, see [Song lengths](song-lengths.md). "Tunes only" hides everything but the songs.

Right-click a row to add it to a playlist or export it.

## The chip panel

One panel per SID chip in the tune, showing the three voices, their waveforms, control bits and pitch, the filter and its mode, live. The tooltip names the chip model and the [chip profile](chip-profiles.md) if one is active. Tunes that use sample playback get a separate strip showing the digi output.

Above it, a memory map of the C64 with the tune's location. Hover it for the load, init and play addresses and the name of the player routine, identified from a signature database.

Below, a stereo spectrum with a marker per voice at its current pitch.

## What ultraSID deliberately does not do

- **No editing STIL.** The notes are the HVSC's. Corrections go to the HVSC team, and everyone gets them with the next release.
- **No lyrics, no cover art per tune.** The title screens and game screenshots on the CRT page are the artwork. See [CRT emulation](crt.md).
