# X4 Pro Play+Companion r7

r7 keeps the known-good r6 firmware behavior and adds a user-editable PNG asset pipeline for **all companions**.

## Editable Companion artwork

Artwork lives under `assets/companions/`:

- `sophocles/` — fox
- `vellum/` — ghost
- `octavo/` — robot
- `noodle/` — dog

Each folder contains four PNG poses: `thriving.png`, `content.png`, `peckish.png`, and `neglected.png`. During every PlatformIO build, `scripts/gen_companion_sprites.py` reads those PNGs and regenerates `src/companion/CompanionSprites.generated.h` automatically.

The existing `.grid` files in `src/companion/sprites/` remain the source for character names, species labels, and dialogue, and also act as a legacy art fallback if the PNG asset folder is absent.

## Build

Default environment: `x4pro_r7`

Output: `dist/x4pro-playcompanion-r7.bin`
