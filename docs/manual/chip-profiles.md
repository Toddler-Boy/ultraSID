---
title: Chip profiles
---

# Chip profiles

A chip profile tells the emulation which 6581 to be. ultraSID ships with one for each of the most popular composers, and picks it automatically when you play their tunes.

## Why one 6581 is not enough

The 6581 filter was never built to a spec. It depends on the chip revision, the batch, the age of the chip and how warm the machine is. Two C64s from the same year can sound noticeably different on the same tune, and the filter sweeps composers loved so much are exactly where the difference is largest.

A composer wrote and mixed on one particular chip. Play the tune on any other and the filter opens at the wrong point, the bass is thinner or fatter, the resonance peaks somewhere else. It is still the tune, but it is not the mix.

## What a profile holds

A profile is a small set of numbers that describe one chip: the filter curve, gain and saturation, the resonance, the DC offsets that decide how loud sample playback comes out, and how strongly combined waveforms bleed into each other. Together they pin the emulation to that composer's machine. There is also a charge-leakage rate, which no shipped profile changes from the default.

ultraSID ships 81 of them. A profile applies to a composer's folder in the HVSC, so every tune by that composer gets it without you doing anything. Collaborations can be routed to the right co-author's chip per tune.

Five profiles carry the composer's own approval: Martin Galway, Matt Gray, Chris Hülsbeck, Reyn Ouwehand and Jeroen Tel listened to their tunes in ultraSID and signed off on the sound. The chip panel in the right sidebar says "Approved by" when one of these is playing, and shows the composer's portrait.

## What you see and hear

- The chip panel shows a small indicator when a profile is active, and the tooltip names it. Without a profile the tooltip reads "Excellent 6581 emulation", which is the stock chip.
- Profiles only shape the 6581. The 8580 was a far more consistent chip, so 8580 tunes always play on the stock emulation.
- Tunes outside the HVSC musicians folders, including your own dropped-in files, play on the stock chip.

## What you can change

- **Ctrl+Shift+F9** opens the chip profile editor while a tune plays. It exists for SID authors who want to match their own chip. Changes are audible immediately, and "Save to user" keeps them in your data folder, where they override the shipped profile of the same name.

## What ultraSID deliberately does not do

- **No global 6581 / 8580 switch.** The tune's header decides the chip. Forcing an 8580 tune onto a 6581 gives a sound the composer never heard.
- **No filter sliders on the settings page.** Filter settings belong to the chip a tune was written on, not to your taste. The editor is there if you know what you are doing, and hidden for a reason.
- **No profiles for the 8580.** There is nothing to profile.
