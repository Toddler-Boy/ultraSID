# Changelog

Headline changes only: small fixes and internal work are not listed.

## 85.2.5 (2026-09-20)

- Fixed the menus showing up black on macOS, the previous fix did not reach the
  release build.

## 85.2.4 (2026-09-19)

- Fixed the menus showing up black on macOS.

## 85.2.3 (2026-09-19)

- Fixed the auto-update on Windows. This version has to be downloaded and installed
  by hand once.

## 85.2.2 (2026-09-18)

- Search by year: "1987", "198?", "1985-1989", "-1990" or "1990-" narrows the results
  to those release years.
- A search word can be limited to one field with a prefix: name:, author:, path:,
  publisher: or year:. Quotes keep a phrase together.

## 85.2.1 (2026-09-17)

- Fixed a potential crash while closing ultraSID. The window now closes instantly.

## 85.2.0 (2026-09-17)

- Playlists sort by name, release, chip or length with a click on the column header.
  "Keep this order" in the playlist menu makes the sorting permanent.
- Playlists: the playlist menu can remove duplicates and shuffle the order.
- The update check can run at every start.
- Screen reader support: tune lists, the STIL subtune list, buttons, search results,
  playlist cards, tags and chip names are announced properly.

## 85.1.2 (2026-09-14)

- The equalizer moved from the settings page into a popup in the footer, next to the
  volume control (Ctrl+Shift+E or Ctrl+G toggles it).

## 85.1.1 (2026-09-12)

- Linux: fixed menus and dropdowns showing up as a black box under KDE Plasma.
- Fixed the HVSC update getting stuck on the progress page after it had finished.

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
