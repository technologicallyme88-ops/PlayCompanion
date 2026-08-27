# Companion artwork

This is the editable artwork folder for every Companion in Play+Companion r8.

## Characters

- `sophocles/` — fox
- `vellum/` — ghost
- `octavo/` — robot
- `noodle/` — dog

Each character has four required files:

- `thriving.png`
- `content.png`
- `peckish.png`
- `neglected.png`

## Replacing a picture

Replace the PNG for the pose you want to change and build the firmware normally.
PlatformIO automatically runs `scripts/gen_companion_sprites.py`, which converts
these images into the packed 1-bit sprite table compiled into the firmware.

You do not need to edit `CompanionSprites.generated.h`.

### Recommended artwork

For pixel-perfect control, use a **34 x 30 px** PNG with a transparent or white
background and solid black artwork. A 34x30 image is used pixel-for-pixel.

Larger PNGs are also supported. The build script finds the visible dark artwork,
fits it into the 34x30 sprite canvas **without changing its aspect ratio**, and
centers it. This makes it possible to draw or edit a character at a comfortable
resolution without accidentally stretching it.

The converter treats transparent/near-white pixels as paper and dark pixels as
e-ink. Use non-interlaced 8-bit PNGs (normal PNG exports from common image
editors meet this requirement).

## Names and dialogue

Character names, species labels, and quotes are still kept in:

`src/companion/sprites/<character>.grid`

The old grid artwork is retained only as a fallback. When the PNG folder exists,
the PNGs are the artwork that gets compiled.


## r9 supplied-grid refresh
The r9 checked-in PNG poses were regenerated from the supplied `.grid` artwork using the firmware's exact dither rules. The PNGs remain editable for future builds.
