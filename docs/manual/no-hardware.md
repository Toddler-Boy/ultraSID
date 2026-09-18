---
title: Why ultraSID does not play through SID hardware
---

# Why ultraSID does not play through SID hardware

The most requested feature, by far, is playback through a real SID: a USBSID-Pico, a C64 Ultimate, a SIDBlaster. ultraSID will not get it. This page explains why, and why you are not missing anything.

## The emulation is not the compromise

The usual reason for wanting hardware is the belief that emulation is close, but not close enough. That was true fifteen years ago. It is not true now.

ultraSID uses cycle-exact emulation of the CPU and both SID models. The differences to a real chip have become so subtle that in a blind comparison you cannot tell which is which. New sample-playback techniques that the scene keeps discovering, tricks nobody designed the emulation for, play correctly without a single change to the code. That is how close it is.

## There is no "the real thing"

A real 6581 is not one sound. Every chip differs by revision, by production batch, by age and by temperature, and the filter is the part that differs most. Two C64s side by side play the same tune differently. The chip the composer had on the desk in 1987 is a chip nobody has.

A hardware SID gives you one chip, the one you happen to own. That is a sound, but it is not the original.

ultraSID goes the other way. Each popular composer has a chip profile, a set of filter and DC measurements that reproduce the chip they wrote on. Some of these profiles were checked by the composers themselves: Martin Galway, Matt Gray, Chris Hülsbeck, Reyn Ouwehand and Jeroen Tel listened to their own tunes in ultraSID and signed off on the result. When the chip panel says "Approved by", you are closer to the composer's machine than any chip you can buy.

See [Chip profiles](chip-profiles.md).

## What hardware would take away

Everything ultraSID does beyond playing a tune depends on having the audio as data before you hear it. A hardware SID produces sound in real time, on the far side of a cable, and none of this works anymore:

- **Chip profiles.** Hardware plays on whatever chip is in the socket.
- **Consistent loudness.** The level of every tune is measured before playback. See [Loudness](loudness.md).
- **Scrubbing.** Tunes are rendered ahead of the playhead, which is why you can jump anywhere instantly. Hardware can only fast-forward by playing faster. See [Pre-rendering](pre-rendering.md).
- **Exports.** WAV, FLAC and OGG come from the same render. See [Exports](exports.md).
- **Audio enhancements.** The stereo placement, the equalizer and the Magic, Epic and Mythic modes all act on the rendered audio. See [Audio quality](audio-quality.md).
- **Any number of chips.** Ten-SID tunes play as written. Hardware has one chip, or two with luck.

Supporting hardware would mean a second player inside ultraSID with most features greyed out. That is a different app, and a worse one.

## If you want to hear your own chip

Use a player built for that. ultraSID is built for the collection, the playlists and the composer's sound, and it stays that way.
