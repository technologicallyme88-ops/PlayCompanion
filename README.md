# PlayCompanion Firmware

PlayCompanion is a CrossPlay fork for Xteink e-readers that combines companion reading
and Pokémon companion reading, while introducing a Reading Journal alongside
the EPUB reader, SD-card library, sync, settings, and offline game shelf.

## Supported builds

| Device | Environment | Hardware |
| --- | --- | --- |
| X4 Pro | `x4pro_r11_5` | ESP32-S3, touch |
| X4 | `x4_r11_5` | ESP32-C3, buttons |
| X3 | `x3_r11_5` | ESP32-C3, buttons |

The X3 and X4 builds include button navigation for local games. The X4 Pro uses
touch controls, with button support where the device exposes it.

## Features

- EPUB reading with progress tracking and Reading Journal history
- Modified CrossPlay game shelf, adding Dungeon Run and removing some CrossPlay games
- Pokémon party, companion selection, encounters, and reading-linked progress
- Reading companions with moods, artwork, and dialogue
- USB and Bluetooth status indicators where supported
- Offline-first storage on the device SD card

## Building

Install PlatformIO and build the target for the device you are flashing:

```powershell
pio run -e x4pro_r11_5
pio run -e x4_r11_5
pio run -e x3_r11_5
```

Generated firmware files are under `.pio/build/<environment>/`. Confirm the
target before flashing: X4 Pro uses ESP32-S3, while X3 and X4 use ESP32-C3.

## Repository layout

- `src/activities/` — reader, home, settings, and Pokémon activities
- `src/apps_local/` — games and the game shelf
- `src/companion/` — companion state, rendering, sprites, and dialogue
- `src/pokemon/` and `lib/Pokemon/` — Pokémon service, game state, and UI logic
- `scripts/` — build-time generation and packaging helpers
- `docs/` — development and app-specific documentation

Read [AGENTS.md](AGENTS.md), [LOCAL_SCOPE.md](LOCAL_SCOPE.md), and
[docs/shelf.md](docs/shelf.md) before making changes.

## Licensing

PlayCompanion is distributed under the MIT License. See [LICENSE](LICENSE) and the
third-party notices under `freeink-sdk/` for dependency-specific terms.

PlayCompanion is an independent community fork derived from CrossPlay and is not
affiliated with Xteink, Nintendo, Pokémon, or the publishers and owners of the
included games.
