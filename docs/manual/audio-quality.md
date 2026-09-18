---
title: Audio quality
---

# Audio quality

The button next to the volume control picks one of five modes. They decide what happens to the chip's output before it reaches your speakers.

## The five modes

- **Real.** The chip through a tiny monitor speaker: harsh, distorted, with transformer hum and hiss. This is what most people actually heard in the eighties.
- **Pure.** The chip and nothing else. Only inaudible subsonics are removed. Bit for bit, this is the raw emulation.
- **Magic.** Mild stereo widening, a light delay and reverb, and a gentle bass shelf. The default, and the recommendation.
- **Epic.** The same chain, turned up: wide stereo, deep delay and reverb, bold bass.
- **Mythic.** Everything cranked to 11. Maximum width, endless echoes, cavernous reverb.

Pure, Magic, Epic and Mythic are one chain at four intensities. Real is a different chain, built to sound like a small speaker rather than to enhance anything.

Switching modes glides instead of jumping. "Quality transition time" in Settings sets the seconds per step, 0.3 by default; set it to 0 for instant switches. Ctrl+E opens the popup, and the up and down keys step through the modes while it is open.

## The equalizer

The EQ button next to it opens a three-band equalizer: low shelf at 200 Hz, a mid band at 1 kHz, high shelf at 4 kHz, each up to 12 dB in either direction. It sits on top of whatever the mode does, so it works the same in all five. Drag a handle, read the value in the bubble, Alt+click a handle to reset it.

## Stereo placement

A SID chip is mono. Multi-chip tunes can be spread across the stereo field, but not all of them should be: some were written with the chips as one instrument, and pulling them apart wrecks the mix.

ultraSID ships a curated list of multi-chip tunes with a stereo width that suits each one. Tunes on the list get their width. Multi-chip tunes not on the list stay mono, unless the tune itself declares where its chips sit. The "Stereo placement" setting under Audio Enhancements turns the curated widths off, which folds everything to mono.

## What ultraSID deliberately does not do

- **No reverb, delay or width knobs.** The four intensities are the knobs. Individual controls would let you build a mix that sounds wrong on every tune, and nobody could tell you which setting is right.
- **No "make it sound like my monitor" presets.** Real is the one speaker simulation, and it is deliberately a bad speaker.
- **No effects on hardware output.** Every mode processes the rendered audio. See [Why ultraSID does not play through SID hardware](no-hardware.md).
