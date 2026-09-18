---
title: Song lengths
---

# Song lengths

A SID tune has no end. It is a program that loops until the machine is switched off. Everything about when a tune stops, repeats or fades is decided by data and by four settings.

## Where the length comes from

The HVSC ships a length for every subtune in the collection, measured by the HVSC team over the years. ultraSID reads that file and adds a small addendum for the handful of tunes the HVSC does not carry yet. When you play a tune, it plays for that length and then fades out.

Tunes you drop in yourself have no known length. They play for the "Unknown tune length" setting, 5 minutes by default.

## The four settings

All under Settings, Song Lengths:

- **Unknown tune length** (1 to 20 minutes, default 5). How long to play a tune nobody has measured.
- **Minimum length** (0 to 600 seconds, default 60). A short tune keeps looping until this much time has passed. A 12-second jingle plays five times over.
- **Maximum repeats** (0 to 99, default 5). Caps the looping. A 4-second sound effect with a 60-second minimum would loop fifteen times; with the cap at 5 it stops after 20 seconds. Set to 0 to remove the cap.
- **Fade out** (0 to 60 seconds, default 10). Added to the end, and the tune fades to silence during it.

The order is: take the known length, stretch it to the minimum, cap the stretch by the repeats, then append the fade.

## Tunes that end by themselves

Some tunes stop on their own: a jingle, a game-over sting, a sample that plays once. ultraSID knows which ones, from the scan that built its database. These one-shot tunes play exactly once, get no fade, and are not stretched to the minimum. There would be nothing to fade or repeat, only silence.

## Silent intros

About 1 500 tunes in the collection start with silence, sometimes seconds of it, because the player routine needs time to set up or the composer left a gap. ultraSID has a list of them and skips the lead-in, so the tune starts when the music does. The length shown is the length of the music.

## Stingers, effects and speech

The sub-tune list in the right sidebar sorts every subtune into one of four kinds by its length: songs, stingers (under 20 seconds), sound effects (under 5 seconds), and speech samples, which are the ones whose STIL name is in quotes. The "Tunes only" toggle hides everything but the songs.

## What ultraSID deliberately does not do

- **No "play forever".** Set the minimum length to 600 seconds and the repeats to 0 if you want ten minutes of a jingle. Beyond that, put it on repeat.
- **No manual length per tune.** The HVSC length is the community's measurement. If it is wrong, the fix belongs in the HVSC, and it will arrive with the next update.
