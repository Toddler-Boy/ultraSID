---
title: CRT emulation
---

# CRT emulation

The CRT page shows the title screen or a game screenshot for the tune that is playing, on an emulated monitor. Not a filter over a picture: the image is encoded to a C64 video signal, decoded the way a PAL or NTSC set decodes it, and drawn on a tube with a phosphor mask, curvature and glass.

## Why not just show the screenshot

A C64 screenshot is a grid of 16 flat colours. Nobody ever saw it that way. What people saw was that signal through a composite cable into a television or a monitor, with colour bleeding into neighbouring pixels, PAL's alternating lines blurring into new colours, scanlines, a slight curve, and the room reflected in the glass. The artwork was drawn for that, and looks wrong without it.

## What is emulated

- **The signal.** PAL or NTSC, chosen by the tune's clock or forced. The palette comes from Colodore, the VIC-II colour model by Philip Timmermann, so the base colours are right before any degradation starts.
- **The decoding.** Luma and chroma blur, crosstalk, colour fringing, PAL phase and Hannover bars, NTSC rainbowing, drift and noise.
- **The tube.** Curvature, rotation, convergence, deflection waver, scanlines, a choice of phosphor masks (slot, shadow, aperture grille, the SX-64's own), phosphor decay, halation, adjacent-cell excitation, vignette.
- **The room.** A photographed monitor as the bezel, with time of day, dust and a reflection layer. With the webcam enabled, the reflection is your actual room.

## Presets

The Preset picker under CRT Emulation sets everything at once:

- **Default**: a good PAL monitor, the way most people remember it.
- **SX-64**: the tiny built-in screen of the portable, with its own mask.
- **VICE**: the clean picture of a software emulator. Scanlines, nothing else.
- **C64 Ultimate HDMI**: a modern digital output. Flat, sharp, no artefacts.
- **Cheap and broken TV**: every defect turned up.

Any change to a slider makes the preset "Custom". "Save preset…" keeps your own; it lands in your data folder and shows up in the picker. Double-click or Alt-click a slider to reset it.

## The artwork

Nearly all title screens and game screenshots come from Lemon64, with their kind permission. Around 2 000 tunes have artwork, many with several screens; the dots under the picture switch between them, and the choice is remembered.

Tunes without artwork get a generated player screen instead: a C64 screen drawn live, showing the tune, the author, the chip, the addresses, the progress, and a per-voice frequency readout, in the style of a BASIC listing. Settings picks which layout, or Random.

## Fullscreen

F11, Alt+Enter, or a double-click on the picture. The mouse wheel zooms the overlay, Ctrl and the wheel adjusts overscan.

## What ultraSID deliberately does not do

- **No plain image mode.** Set the preset to C64 Ultimate HDMI and switch the overlay off. That is as plain as a C64 picture gets.
- **No screenshot uploads.** Artwork is curated and ships with the app.
