---
title: Where your data lives
---

# Where your data lives

Everything you make in ultraSID lives in one folder, in plain files you can read.

## The data folder

By default it is `ultraSID user-data` in your Documents folder. Settings, My Data, shows where it is and lets you change it or move it. "Move location" copies everything to an empty folder of your choice and removes the old one afterwards.

Inside:

- `Playlists/`: one `.m3u` per playlist, with a `#PLAYLIST:` line for the name. A `.jpg` or `.png` of the same name next to it is the cover.
- `Tunes/`: the `.sid` files you dropped in.
- `likes.txt`, `history.csv`, `exports.csv`: your likes, your play history, your export queue.
- `preferences.yml`: every setting from the settings page, the footer and the CRT panel.
- `chip-profiles.csv`: your own chip profiles, if you saved any from the editor.
- `SID_LUFS.txt`: loudness measurements for your own tunes.
- `Themes/`, `Overlays/`, `CRT Masks/`, `CRT Presets/`: your own additions. Anything you put there shows up in the pickers right away.

The folder is watched. Drop a file in, or edit one, and ultraSID picks it up without a restart.

The only things outside it are window position, the last page you were on and similar bookkeeping in the operating system's app-data folder, plus the log file. Nothing you would want to keep.

## Playlists are just m3u

A playlist is an ordinary `.m3u` with HVSC-relative paths. You can hand one to someone else, and they can drop it onto their ultraSID. The playlists on the ultraSID website work that way. Drag one from the browser straight into the app.

## Backup and moving machines

Settings, My Data, "Export…" packs the categories you tick into one zip: playlists, likes, history, your tunes, themes, CRT files, preferences. "Import…" brings such a zip back in, and asks whether to merge with what you have or replace it. Merging keeps everything and adds the archive's content.

That zip is the whole of your ultraSID life. Keep one.

## Hand-editing

Both `preferences.yml` and the bookkeeping file are plain YAML and rewritten on exit. Edit them while ultraSID is closed if you need to. Values out of range are clamped on the next start.

## What ultraSID deliberately does not do

- **No cloud sync.** Put the data folder in a synced folder if you want that. It is plain files, and it works.
- **No database you cannot read.** The shipped tune database is inside the app and never changes. Everything of yours is text.
