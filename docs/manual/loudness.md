---
title: Loudness
---

# Loudness

Every tune in the collection plays at the same perceived volume. That is the "Normalize volume" setting under Playback, on by default.

## The problem

SID tunes have no volume standard. One composer drove the mixer to the limit, the next left half the headroom unused, and a third wrote a quiet intro before the drop. Play a playlist across composers without correction and you spend the evening reaching for the volume knob.

## What ultraSID does

Every tune and every subtune in the HVSC has been measured in advance, the way broadcast and streaming loudness is measured: integrated loudness over the whole tune, in LUFS, on the raw chip output. The measurement ships inside ultraSID. When a tune starts, its gain is set so it lands on the common target, and stays there for the whole tune.

The correction is a fixed gain per tune, nothing else:

- **No limiter, no compressor.** A tune with a quiet intro keeps its quiet intro and its loud drop. Only the overall level moves.
- **Boost is capped.** A tune can be raised by 20 dB at most, so a nearly silent oddity does not become a wall of hiss.
- **Applied before the audio enhancements.** The Magic, Epic and Mythic modes and the equalizer all get an input at the same level, so they behave the same on every tune.

Tunes you drop in yourself have no measurement. They are measured while they play the first time, the result is kept in your data folder, and the second play is correct from the first sample.

## What you notice

- Playlists across composers sit at one level.
- Quiet tunes do not pump or breathe. If a tune feels too soft, that is its dynamics, not a broken measurement.
- Turning the setting off gives you the raw chip level, which is what you would get from the hardware.

## Not the same as export normalizing

The export page has its own "Normalize volume". That one is a plain peak normalize on the finished file, so the loudest sample sits just below full scale. It makes a single file as loud as it can be without clipping. It does not make files equal to each other. Loudness matching in exports comes from the playback setting, which the export applies too.

## What ultraSID deliberately does not do

- **No per-tune volume knob.** It would be a manual version of the measurement, done worse, and lost the moment you play the tune on another machine.
- **No "louder" target.** Louder is a limiter, and a limiter changes the music.
