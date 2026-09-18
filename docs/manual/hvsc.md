---
title: The collection
---

# The collection

ultraSID plays the High Voltage SID Collection, the HVSC: every C64 tune anyone has managed to preserve, organised by composer and game, with lengths and liner notes. Nothing else comes close, and ultraSID is built around it.

## ultraSID installs it for you

On first launch you get a choice: point ultraSID at a copy you already have, or click "Download HVSC for me". The download is under 100 MB and comes from ultraSID's own server, so it is fast and always the release the app expects.

You can keep the collection as a folder or as a single zip. ultraSID reads both the same way. The zip saves you 60 000 small files on disk; the folder lets you browse it.

## Updates

The HVSC publishes a new release twice a year, in June and December, with an official update package that patches the previous one. ultraSID applies exactly those packages, in order, the way the HVSC team intends. If your collection is several releases behind, they are applied one after the other. If it is too old to patch, ultraSID downloads the full collection instead.

The update is staged first and swapped in only when it has completed. If anything goes wrong halfway, the old collection is untouched.

Each ultraSID release is built for one HVSC release, because the tune database that ships inside the app, the loudness measurements, the one-shot flags, the silent intros, is generated from that exact collection. So a new HVSC release arrives as a new ultraSID release, the same day or the day after, and the "HVSC is outdated" screen appears once, offering to update.

## The exotic ten

Ten tunes in ultraSID are not in the HVSC. They use four, eight or ten SID chips, in a variant of the PSID format that the HVSC has decided not to recognise, and so they have no home there. ultraSID ships them inside the app, in the folder where they would sit in the collection, with lengths, and the search finds them like any other tune. They include the ten-chip Nutcracker, The Tuneful Eight and the 4SID work by Rayden and Vincenzo.

The format allows any number of chips and ultraSID has no limit either. The practical ceiling is how many chips a single C64 can feed within one frame, and ten is probably it.

## Your own tunes

Drop a `.sid` file onto the window and it moves into the Tunes folder in your data folder. From there it shows up in search like anything else, and you can drag it into playlists. Links to `.sid` files, and any csdb.dk link, work the same way: dropped on the window, downloaded, added.

Your own tunes have no HVSC data behind them. No known length, so they play for the "Unknown tune length" setting. No STIL notes. No chip profile, so they play on the stock chip. Their loudness is measured while they play the first time.

Only files directly in the Tunes folder are scanned, not subfolders.

## What ultraSID deliberately does not do

- **No "open folder" for arbitrary SID collections.** ultraSID is built for the HVSC. Tunes that matter and are not in it belong in your Tunes folder, or better, in the HVSC.
- **No zip drops.** A zip could be anything. Unpack it and drop the `.sid` files.
- **No editing the collection in place.** The HVSC is read-only to ultraSID, so an update can never collide with local changes. Corrections for broken rips are applied in memory as a tune loads; see [Tune fixes](tune-fixes.md).
