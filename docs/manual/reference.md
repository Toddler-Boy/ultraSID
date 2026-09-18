---
title: Reference
---

# Reference

Every key, gesture, setting and file, one line each.

## Keyboard

| Key | Action |
|---|---|
| Space | Play or pause |
| Ctrl+Left, Ctrl+Right | Previous, next tune |
| Shift+Left, Shift+Right | Seek 5 seconds |
| Ctrl+Up, Ctrl+Down | Volume |
| Ctrl+M | Mute |
| Ctrl+S | Shuffle |
| Ctrl+R | Repeat: off, all, one |
| Alt+Shift+B | Like the playing tune |
| Ctrl+E | Audio quality popup |
| Ctrl+Shift+E or Ctrl+G | Equalizer popup |
| Ctrl+F or Ctrl+L | Search |
| Alt+Shift+S | Liked tunes |
| Alt+Shift+J | Jump to the playing tune |
| Alt+Shift+1 | Playlists |
| Ctrl+N | New playlist |
| Ctrl+Z | Undo the last playlist change |
| Ctrl+A, Ctrl+Shift+A | Select all, deselect all rows |
| Enter | Play the selected row |
| F11 or Alt+Enter | CRT fullscreen, on the CRT page |
| Ctrl+, | Settings |
| ? or Ctrl+/ | This list, inside the app |
| Esc | Close About, the shortcut list, or a popup |
| Ctrl+Shift+F9 | Chip profile editor |
| Ctrl+Shift+F10 | Colour adjustments for the user interface |
| Shift+F11 | Log window |

On macOS, Cmd stands in for Ctrl and Option for Alt.

## Mouse

| Gesture | Effect |
|---|---|
| Double-click a row | Play it |
| Click a column header | Sort; a third click restores the original order |
| Click the heart | Like or unlike |
| Right-click a row | Add to playlist, go to folder, export, and more |
| Drag rows | Onto a playlist, or within one to reorder |
| Click the playlist title | Rename it |
| Drop an image on a playlist | Set its cover |
| Click the right-hand time in the footer | Total length or remaining time |
| Hover the seek bar | Time at the cursor |
| Drag the volume to zero | Mute |
| Alt+click an equalizer handle | Reset that band |
| Double-click or Alt-click a CRT slider | Reset it |
| Double-click the CRT picture | Fullscreen |
| Mouse wheel over the CRT | Zoom the overlay; Shift for fine steps; Ctrl for overscan |
| Hover the undo toast | Pause its countdown |
| Click the thumbnail in the footer | Go to the CRT page |
| Click the version pill | Install a waiting update, or check now |
| Click the logo | About |

## Search

| Input | Meaning |
|---|---|
| words | All must match, in any field |
| "quoted words" | Match as a phrase |
| name: author: path: publisher: | Restrict a word to that field |
| 1987 | Released that year |
| 198? | Any year in the eighties |
| 1985-1989, -1990, 1990- | Ranges, open at either end |
| Pioneers, Winners, Gems, Liked | Filter results by tag |

## Drop targets

| Drop | Where | Result |
|---|---|---|
| .sid files | Window | Moved into your Tunes folder |
| .m3u files | Window | Imported as playlists; a same-named image becomes the cover |
| A csdb.dk link, or a link to a .sid or .m3u | Window | Downloaded and handled as above |
| .png or .jpg, or an image link | Playlist page or card | Cover image |
| Rows from any list | Playlist page or card | Added |

## Settings

| Setting | Default | Notes |
|---|---|---|
| High Voltage SID Collection | | A `C64Music` folder or a zip of one |
| My data | Documents/ultraSID user-data | Change or move the folder |
| Export and import | | Zip of playlists, likes, history, tunes, themes, CRT files, preferences |
| Normalize volume | On | Same perceived loudness for every tune; see [Loudness](loudness.md) |
| Sound output device | System default | |
| Unknown tune length | 5 min | For tunes with no known length |
| Fade out | 10 s | Added to the end |
| Minimum length | 60 s | Short tunes loop until this |
| Maximum repeats | 5 | Caps the looping; 0 for no cap |
| 6581 DAC leakage | Off | The DACs never reach true zero, so voices keep sounding faintly after their release; audible in quiet passages |
| Stereo placement | On | Curated stereo width for multi-chip tunes |
| Quality transition time | 0.3 s | Per step when changing audio quality; 0 is instant |
| Exported tunes location | Documents/ultraSID exports | |
| File format | WAV | WAV, FLAC or OGG |
| Normalize volume (export) | Off | Peak-normalize each file |
| File names | {A} - {T} {N} | Author, title, subtune; also {R} release, {Y} year, {Q} quality, {NN} {NNN} padded; / makes folders |
| Theme | default | Your own go in the Themes folder |
| Boot screen | Basic C64 | Shown while a tune loads |
| Player screen | Random | Shown for tunes without artwork |
| Keep screen awake | Off | No screensaver while ultraSID has focus |
| Check for updates online | On | |
| Check frequency | Every start | Or daily, weekly, monthly |
| Show shortcuts | | The key list |
| Add to Start menu, Move to Programs | | Windows only |

## CRT panel

| Group | Controls |
|---|---|
| Overlay | Bitmap, Time of day, Bezel, Shadow, Aberration, Bloom, Dust, Grain, Zoom |
| System | PAL, NTSC or Auto; luma revision; Warmth, Brightness, Contrast, Saturation, Tint, Overscan |
| CRT Emulation | Preset; Jailbars, Noise, Fringing, Luma Blur, Chroma Blur, Crosstalk, Phase and Hannover (PAL), Rainbowing (NTSC), Drift, Curvature, Rotation, Color bleed, Alignment, Deflection, Expansion, Scanlines, Mask and Mask type, Decay, Vignette, Adjacent, Halation, Ambient, Reflection |
| Webcam | Brightness, Contrast, Saturation, Zoom, Camera |

Presets: Default, SX-64, VICE, C64 Ultimate HDMI, Cheap and broken TV, plus your own. See [CRT emulation](crt.md).

## Files

| File | What it is |
|---|---|
| Playlists/*.m3u | Your playlists; a same-named .jpg or .png is the cover |
| Tunes/*.sid | Your own tunes |
| likes.txt, history.csv, exports.csv | Likes, play history, export queue |
| preferences.yml | All settings |
| chip-profiles.csv | Your own chip profiles |
| SID_LUFS.txt | Loudness of your own tunes |
| Themes/, Overlays/, CRT Masks/, CRT Presets/ | Your additions to the pickers |

All of these live in the data folder. See [Where your data lives](data-folder.md).

## Limits

- The window needs 1280 by 700 pixels.
- No command-line arguments and no file associations. Drop files on the window.
- A second copy of ultraSID exits at once.
- No zip drops. Unpack first.
- Media keys on the keyboard are not handled.
- Only the Tunes folder itself is scanned, not its subfolders.
