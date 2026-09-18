---
title: Tune fixes
---

# Tune fixes

The HVSC is a preservation project, and it is very good. It is also 60 000 files ripped from tapes and disks over thirty years, and a few of them are wrong in ways that take a musician's ear to notice. ultraSID carries its own corrections, applied as a tune loads.

## What gets corrected

- **Wrong default subtune.** Many game rips put a sound effect or a jingle first, and the title tune three subtunes in. ultraSID starts the one you came for. Last Ninja opens with the tune everybody knows, not a menu blip.
- **Wrong chip.** Some headers claim a 6581 for a tune that was written on an 8580, or say nothing at all. Where the composer's own releases settle the question, ultraSID plays the tune on the chip it was written for.
- **Wrong clock.** PAL tunes flagged as NTSC play a semitone sharp and too fast. Fixed the same way.
- **Broken data.** A stray byte in a pattern that makes a voice drift, a missing end marker that cuts a pattern short, a filter cutoff that was mangled in the rip. ultraSID patches the bytes in memory as the tune loads. The file on disk is never touched, and the patch is only applied if the bytes are exactly the ones it expects, so it retires itself the moment the HVSC ships a corrected file.
- **Wrong length.** Where a length was measured on a broken version, the corrected length ships with the patch.
- **Sample playback.** Tunes that play digital samples use dozens of different tricks to do it. ultraSID identifies the player routine and knows which one it is looking at, so the digi display shows the samples and not the carrier, and tunes that only look like they play samples are left alone.

## How much of this there is

A few dozen entries in total. Corrections are made when they are certain, backed by the composer's own recordings or by reading the player code, and each one carries a note saying why. The byte patches are written so that a corrected file in a future HVSC release makes them fall away on their own.

## What ultraSID deliberately does not do

- **No "fix" that is a matter of taste.** Speed, chip and default subtune are corrected only where the original is knowable.
- **No changes to the collection on disk.** Every fix lives in the app and is applied on load. Your HVSC stays a clean copy.
