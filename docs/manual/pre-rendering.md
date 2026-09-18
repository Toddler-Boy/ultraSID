---
title: Pre-rendering
---

# Pre-rendering

ultraSID does not play a SID tune the way a C64 does, one cycle at a time in step with the speakers. It renders the whole tune into memory as fast as the machine allows, and plays from that.

## Why

A SID tune is a program. To know what the chip sounds like at 2:30, you have to run the program up to 2:30. On real hardware, or in a player that emulates in real time, "jump to 2:30" means either waiting two and a half minutes or fast-forwarding through them.

Rendering ahead turns the tune into audio, and audio can be seeked. It is what makes ultraSID feel like a music app instead of an emulator.

## What happens when you press play

1. The emulation starts in the background and runs flat out, many times faster than real time.
2. Playback starts as soon as the first samples exist, a few milliseconds later.
3. The render keeps running until the end of the tune, including the fade-out.

On a current machine the render finishes long before the tune does. Longer tunes and tunes with many chips take proportionally longer, but the emulation still outruns playback by a wide margin.

## What you see

- **The seek bar has two fills.** The bright one is where you are. The dimmer one behind it is how far the tune is rendered. The player screen on the CRT page shows the same.
- **You can seek anywhere inside the rendered part**, instantly and with no glitch. Dragging beyond the rendered edge lands you at the edge.
- **Shift+Left and Shift+Right** move 5 seconds and obey the same limit.

## What it gives you

Every feature that treats the tune as audio depends on this:

- Scrubbing and instant seeking.
- [Loudness](loudness.md) measurement on tunes you drop in.
- [Exports](exports.md), which are the same render written to a file.
- The audio enhancements, which process a stream that already exists.

## What it costs

Memory, for the duration of one tune. A five-minute tune is about 50 MB of audio. The buffer is freed when the next tune starts.

## What ultraSID deliberately does not do

- **No fast-forward.** There is nothing to fast-forward through; drag the seek bar.
- **No real-time emulation mode.** It would take away seeking and everything built on the render, and gain nothing you can hear.
