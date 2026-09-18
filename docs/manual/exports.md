---
title: Exports
---

# Exports

Right-click any tune, or the three-dot menu of a playlist, and choose Export. The tune is rendered in the background while you keep listening, and the file lands in your export folder.

## What goes into the file

An export is the same render you hear during playback, written out instead of played:

- **Same emulation, same chip profile, same length.** The tune plays for its known length, the fade-out is included, and the loudness correction is applied.
- **Same audio quality mode.** The mode active when you queue the export is baked in. Pure gives you the raw chip. Magic, Epic and Mythic give you the processed version, and the reverb and delay tails are allowed to ring out at the end of the file instead of being cut.
- **44.1 kHz, 16 bit**, mono or stereo as the tune was rendered.

## Formats

- **WAV**: uncompressed.
- **FLAC**: lossless, about half the size.
- **OGG**: Vorbis, small, at a quality setting that measures transparent on SID material.

The format is set once under Settings, Export. Each queued tune uses the format that was set when it was queued.

## Normalize volume

The "Normalize volume" toggle under Export peak-normalizes each finished file, so its loudest sample sits just below full scale. That is a different thing from the playback loudness setting; see [Loudness](loudness.md). OGG files always get this ceiling because the encoder needs it.

## File names

"File names" under Export is a template. The default is `{A} - {T} {N}`: author, title, subtune number. Other tokens are `{R}` release, `{Y}` year, `{Q}` quality mode, and `{NN}` or `{NNN}` for zero-padded subtune numbers. A `/` in the template creates a subfolder. The preview under the field shows the result for the playing tune.

## The queue

The Export page lists everything you queued, newest first, with a status and a progress bar. Several tunes render in parallel. You can cancel a running export, re-queue a canceled one, and open the finished file in Explorer or Finder from the right-click menu. Quitting ultraSID with exports running asks first.

## What ultraSID deliberately does not do

- **No sample-rate or bit-depth options.** The emulation renders at 44.1 kHz. Upsampling adds nothing.
- **No MP3.** The patents are dead but the codec is worse than Vorbis at every size, and every player reads OGG.
- **No "export the whole HVSC".** It is 60 000 tunes. Export the playlists you actually listen to.
