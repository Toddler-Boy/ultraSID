# Manual inventory (working file, not part of the site)

Tag column: **X** = concept page (the why), **R** = reference row (one line in a table), **H** = how-to step (quick start only), **-** = skip. Suggested tags are filled in. Change any letter, add notes after it.

## Concepts (proposed pages)

| Tag | Page | Covers | Pre-empts |
|---|---|---|---|
| X | Why ultraSID does not drive SID hardware | The hub. Everything below depends on having the audio as data before it plays. | USBSID-Pico, C64u, any hardware output |
| X | Chip profiles | Per-tune curated 6581/8580 profiles, "Approved by" tooltip, composer portraits, DAC leakage toggle, hidden editor (Ctrl+Shift+F9) | "Add a global 6581/8580 switch", "let me pick filter settings" |
| X | Loudness and normalize volume | Per-tune LUFS measurement (SID_LUFS.txt), −18 LUFS target, why not a peak limiter, export normalize is a different thing | "Add a per-tune volume knob", "why is tune X quieter" |
| X | Pre-rendering and scrubbing | Tune renders ahead of playback; seek bar clamped to rendered position; dimmer fill; why seeking is instant | "Add fast-forward", "why can't I seek to the end immediately" |
| X | Audio quality modes | Real / Pure / Magic / Epic / Mythic, what each adds, transition time, EQ on top, stereo placement for multi-chip tunes | "Add a reverb knob", "make it sound like my 1084" |
| X | Exports | Same render path; formats; FX tail; name template; quality captured at enqueue | "Export from hardware", "batch export the whole HVSC" |
| X | Song lengths, fades and repeats | HVSC Songlengths + shipped addendum; unknown length, fade, minimum length, max repeats | "Tune stops too early / loops forever" |
| X | HVSC and your own tunes | HVSC is required; auto download and update; the ten shipped "exotic" multi-chip tunes the HVSC refuses; dropping .sid moves into Tunes folder; csdb.dk links | "Play from any folder", "open a zip" |
| X | STIL and tune info | STIL notes, sub-tune list with stinger/FX/speech icons, memory map, playroutine ID | |
| X | CRT emulation | What is emulated (PAL/NTSC, luma, masks, bezels, webcam reflections), presets, why the picture is not a plain screenshot | "Just show the image", "add a fullscreen without the frame" |
| X | Where your data lives | User folder contents, moving it, export/import zip, settings.yml vs preferences.yml, hand-editing | "Sync between machines", "backup" |
| X | Updates and integrity | Manifest check, SHA-256, signed exe, damaged-file dialog | "Why does it phone home", "portable mode" |
| X | Tune fixes | Byte patches for broken rips, default-subtune / chip / clock overrides, corrected lengths, digi player identification | "Tune X sounds wrong", "why does it start on subtune 3" |

## Quick start (the only how-to)

| Tag | Step |
|---|---|
| H | Download, first launch, Welcome screen: point at existing HVSC or "Download HVSC for me" |
| H | Search: type, filter chips, double-click to play |
| H | Footer: play/pause, seek, quality button, volume |
| H | Make a playlist: + in sidebar, drag rows, right-click "Add to playlist" |
| H | CRT page and fullscreen |
| H | Where to find the keyboard shortcuts (? key) |

## Reference tables

### Keyboard shortcuts

| Tag | Key | Action |
|---|---|---|
| R | Space | Play / pause |
| R | Alt+Shift+B | Like the playing tune |
| R | Ctrl+S | Shuffle |
| R | Ctrl+R | Repeat mode (off, all, one) |
| R | Ctrl+Left / Ctrl+Right | Previous / next tune |
| R | Shift+Left / Shift+Right | Seek 5 s |
| R | Ctrl+Up / Ctrl+Down | Volume |
| R | Ctrl+M | Mute |
| R | Ctrl+E | Audio quality popup |
| R | Ctrl+Shift+E or Ctrl+G | Equalizer popup |
| R | Ctrl+L or Ctrl+F | Search |
| R | Alt+Shift+S | Liked tunes |
| R | Alt+Shift+J | Jump to the playing tune |
| R | Alt+Shift+1 | Playlists |
| R | Ctrl+, | Settings |
| R | ? or Ctrl+/ | Shortcuts dialog |
| R | Ctrl+N | New playlist |
| R | Ctrl+Z | Undo |
| R | F11 or Alt+Enter | Fullscreen CRT |
| R | Esc | Close About / Shortcuts / footer popup |
| R | Ctrl+A / Ctrl+Shift+A | Select / deselect all rows |
| R | Enter | Play selected row |
| R | Ctrl+Shift+F9 | Chip profile editor |
| R | Ctrl+Shift+F10 | Colour adjustments |
| R | Shift+F11 | Log window |
| - | Ctrl+F11, F11 inspector | Developer builds only |

### Mouse gestures

| Tag | Gesture | Effect |
|---|---|---|
| R | Double-click CRT | Fullscreen |
| R | Wheel over CRT | Overlay zoom; Shift fine; Ctrl overscan |
| R | Click right-hand time in footer | Total length vs remaining |
| R | Hover seek bar | Time bubble |
| R | Drag volume to 0 | Auto-mute |
| R | Alt+click EQ handle | Reset band |
| R | Third click on sorted header | Clear sort |
| R | Hover undo toast | Pause countdown |
| R | Click playlist title | Rename |
| R | Click tune thumbnail in footer | Go to CRT page |
| R | Click version pill | Install update or check now |
| R | Click logo | About |

### Search syntax

| Tag | Item |
|---|---|
| R | Words are ANDed; "quoted phrase" |
| R | Prefixes name: author: path: publisher: year: |
| R | Years: 1987, 198?, 1985-1989, -1990, 1990- |
| R | Filter results: Pioneers, Winners, Gems, Liked (and what each means) |
| R | Column sort, third click clears |
| R | Go to folder from right-click |

### Drag and drop

| Tag | Drop | Where | Result |
|---|---|---|---|
| R | .sid files | window | moved into user Tunes folder |
| R | .m3u files | window | new playlist, sibling jpg/png becomes cover |
| R | csdb.dk URL, or URL ending .sid / .m3u | window | downloaded and handled as above |
| R | png/jpg or image URL | playlist page or card | cover image |
| R | rows from any list | playlist page or card | appended / inserted |

### Playlists

| Tag | Item |
|---|---|
| R | Create: + button, Ctrl+N, or "New playlist" in Add to playlist |
| R | Rename by clicking the title |
| R | Reorder by drag; disabled while column-sorted; "Keep this order" freezes a sort |
| R | Three-dot menu: Export playlist, Keep this order, Remove duplicates, Shuffle, Delete cover-image, Delete playlist |
| R | Row menu: Add to playlist, Remove, Go to folder, Export, Move to top / bottom |
| R | Undo toast for all destructive actions, Ctrl+Z |
| R | Stored as .m3u with #PLAYLIST: header in user folder; cover is sibling jpg/png |
| R | Missing tunes stay listed in red |

### Right sidebar

| Tag | Item |
|---|---|
| R | STIL toggles: Tunes only, Notes, Chips |
| R | Sub-tune list: double-click plays once and detaches from playlist; heart per sub-tune; half-heart tooltip |
| R | Memory map tooltip: load / init / play address, playroutine, speed |
| R | Chip panels: one per SID, tooltip "Excellent 6581 emulation" or "Approved by" |
| R | Digi display appears only for digi tunes |
| R | Stereo spectrum with per-voice pitch markers |

### Footer

| Tag | Item |
|---|---|
| R | Shuffle only enabled when playing from a playlist |
| R | Repeat: off, all, one; a single tune counts as a playlist of one |
| R | Seek bar clamped to rendered-ahead position |
| R | Quality popup: five modes with one-line descriptions; Up/Down while open |
| R | EQ popup: LOW / MID / HIGH, dB readout, stays open |

### History

| Tag | Item |
|---|---|
| R | Newest first, relative dates |
| R | Keeps 300 entries / 365 days |
| R | Clear older than 1 / 3 / 6 months / 1 year; Clear history; Remove from history |
| R | Playing from History does not add a new entry |

### Export queue

| Tag | Item |
|---|---|
| R | Statuses: New, Rendering, Applying FX, Saving, Complete, Canceled, Paused, Can't write to path |
| R | Show in Explorer / Finder; Cancel; re-queue canceled |
| R | Re-queued rows render with current settings |
| R | Quit while exporting asks for confirmation |
| R | Name template tokens {A} {T} {R} {Y} {Q} {N} {NN} {NNN}, slash makes subfolders |

### Settings, one row each

| Tag | Setting | Values / default |
|---|---|---|
| R | HVSC location | folder or HVSC.zip |
| R | My data location, Change / Move | Documents/ultraSID user-data |
| R | Export and import (7 categories, Merge / Replace) | |
| R | Normalize volume | On |
| R | Sound output device | System default; always 44.1 kHz internally |
| R | Unknown tune length | 1–20 min, 5 |
| R | Fade out | 0–60 s, 10 |
| R | Minimum length | 0–600 s, 60 |
| R | Maximum repeats | 0–99, 5 |
| R | 6581 DAC leakage | Off |
| R | Stereo placement | On |
| R | Quality transition time | 0–10 s, 0.3 |
| R | Exported tunes location | Documents/ultraSID exports |
| R | File format | WAV / FLAC / OGG, WAV |
| R | Normalize volume (export) | Off |
| R | File names | {A} - {T} {N} |
| R | Theme | default + user themes |
| R | Boot screen | Basic C64 |
| R | Player screen | Random |
| R | Keep screen awake | Off |
| R | Check for updates online | On |
| R | Check frequency | Every start / Daily / Weekly / Monthly |
| R | Show shortcuts | |
| R | Add to Start menu / Move to Programs (Windows) | |

### CRT settings panel

| Tag | Group | Controls |
|---|---|---|
| R | Overlay | Bitmap, Time of day, Bezel, Shadow, Aberration, Bloom, Dust, Grain, Zoom |
| R | System | PAL/NTSC/AUTO, luma revision, Warmth, Brightness, Contrast, Saturation, Tint, Overscan |
| R | CRT Emulation | Preset (5 factory + user, Save preset), Jailbars, Noise, Fringing, Luma/Chroma Blur, Crosstalk, Phase, Hannover, Rainbowing, Drift, Curvature, Rotation, Color bleed + RGB pads, Alignment, Deflection, Expansion, Scanlines, Mask + type, Decay, Vignette, Adjacent, Halation, Ambient, Reflection |
| R | Webcam | Brightness, Contrast, Saturation, Zoom, Camera |
| R | Double-click or Alt-Click a slider resets it; user overlays / masks / presets from the user folder appear immediately |

### Files and folders

| Tag | Item |
|---|---|
| R | User folder: Playlists/, Tunes/, Themes/, Overlays/, CRT Masks/, CRT Presets/, likes.txt, history.csv, exports.csv, SID_LUFS.txt, preferences.yml, chip-profiles.csv |
| R | settings.yml and log.txt in the OS app-data folder (per-platform paths) |
| R | Both yml files hand-editable while the app is closed |
| R | Folder watcher: Tunes, Themes, presets, overlays, masks, chip-profiles.csv reload live |

### Not supported (worth saying once)

| Tag | Item |
|---|---|
| R | No command-line arguments, no file associations |
| R | Second instance exits silently |
| R | No zip drop |
| R | No global media keys |
| R | Window minimum 1280×700; sidebars fixed width |
| R | Changelog is on the website only, not in the app |

### Hidden windows

| Tag | Item |
|---|---|
| R | Chip profile editor fields (for SID authors): filter curve, gain, saturation, resonance, wave DC, EXT-IN DC, voice bias, leakage rate, old filter cap, combined waves, saw-pulse ultra, loop region, Save to user |
| R | Colour adjustments: Gamma, Brightness, Contrast, Saturation, Reset |
