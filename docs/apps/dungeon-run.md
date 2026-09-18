# Dungeon Run

Dungeon Run is an original, compact adaptation of the paper maze-crawler loop
described in *Paper Apps LABYRINTH Detailed Guide*. It keeps the systems that
translate well to an e-reader—two mapped levels, persistent enemy wounds, d6
combat, HP, coins, keys, weapon upgrades, a shield, quick travel, death tally,
and a final guardian—while replacing the source's page-number maze and names
with a fixed sixteen-room campaign designed for this device.

The two floors include four optional room types beyond the original route:

- The Spike Pit traps the player until an escape roll succeeds. Failed rolls
  cost one HP, and the Master Map stays unavailable while trapped.
- The Rune Lock presents a three-choice puzzle and awards six coins once.
- The Forgotten Shrine restores up to four HP once.
- The Hidden Treasury awards eight coins once.

Source supplied for the adaptation:

- <https://docs.google.com/document/d/1Vi0crLbWGsHnVEMPCwu6dBtDjp8GihRM3DjWosadoJw/>

The implementation is split three ways:

- `DungeonRunCore.*`: deterministic, renderer-free rules and fixed flash data.
- `DungeonRunScreens.*`: FreeInkUI/Toybox screen builders.
- `DungeonRunActivity.*`: input, lifecycle, and throttled SD persistence.

The version 2 save is a fixed-size value record at
`/.crosspoint/dungeon-run.sav`. Version 1 saves are rejected because the fixed
room-state array grew from twelve to sixteen entries. No
gameplay path allocates heap memory; only the standard nothrow Activity factory
allocates the activity itself.
