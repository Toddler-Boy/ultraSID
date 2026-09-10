# Changelog

Headline changes only: small fixes and internal work are not listed.

## 85.1.0 (2026-09-10)

- Linux version, as an AppImage for Ubuntu 22.04 or later (and other distributions
  from that era on). Same features, same self-update as Windows and macOS.
- Repeat all now loops a single tune when no playlist is playing.
- Camera: a camera that fails to open is retried quietly, and one plugged in later is
  picked up.

## 85.0.2 (2026-09-07)

- Fixed some unexpected jumps in the playlist overview.

## 85.0.1 (2026-09-07)

- Fixed the last page not being restored at startup. It always landed on search.
- Chip profiles and bug reports without a portrait show a placeholder instead of
  nothing.
- Fixed a crash when the program was moved while running.

## 85.0.0 (2026-09-05)

- Initial release, built against HVSC 85.
- The High Voltage SID Collection installs itself and stays up to date. Nothing to
  download by hand.
- Search as you type: title, author or game, the whole collection narrows down while
  you type.
- STIL notes right next to what's playing: who wrote it, what it was for, what the
  author remembers.
- Playlists, favorites and history. Drag tunes in, drag them around, select a bunch
  at once.
- Scrubbing and song lengths: jump to any point, tunes end when they should.
- Title screens and game screenshots on an emulated CRT, matched to the tune. Nearly
  all screenshots come from Lemon64.com, with their kind permission.
- Live chip view: three voices, the filter and the digi samples on a chip you can
  see.
- Chip profiles for the most popular authors, so their tunes sound the way they did
  on their own C64. Several composers signed off on theirs.
- Same perceived loudness for every tune. Optional.
- Audio enhancements in three doses, Magic, Epic or Mythic: stereo width, delay,
  reverb and bass. Optional.
- Curated stereo placement for multi-chip tunes that suit it. Optional.
- No chip limit: exotic tunes play as written, even the ten-chip ones.
- Export to WAV, FLAC or OGG, single tunes or whole playlists, rendered in the
  background.
- Cycle-exact SID and CPU emulation, curated 6581 profiles, PAL and NTSC timing.
