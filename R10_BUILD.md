# Play+Companion r10 — faster PlatformIO workflow

r10 keeps the r9 device behavior and companion artwork while reducing repeated PlatformIO setup and code-generation work.

## Fast task discovery

The main `platformio.ini` intentionally exposes only five environments:

- `x4_r10`
- `x4_r10_debug`
- `x4pro_r10`
- `x4pro_r10_debug`
- `build_all_r10`

Unrelated upstream boards and desktop simulators are no longer loaded into the normal VS Code Project Tasks list. The previous complete configuration is retained as `platformio.full.ini`, and simulator definitions remain in `platformio.sim.ini` for developers who need them.

## Build All

Use **PlatformIO > Project Tasks > build_all_r10 > Build**. The wrapper launches one PlatformIO command containing both environments, instead of starting PlatformIO separately for X4 and X4 Pro.

Outputs:

- `dist/x4-playcompanion-r10.bin`
- `dist/x4pro-playcompanion-r10.bin`

## Incremental-build optimizations

- HTML gzip output is deterministic (`mtime=0`) and generated headers are only rewritten when their bytes change.
- i18n generated files are only rewritten when their contents change.
- wolfSSL settings are only written when the compatibility patch is actually absent/outdated.
- Companion sprite headers and preview images retain their existing write-if-changed behavior.
- JPEGDEC remains an idempotent in-place patch with no `git apply`/unlink operation.
- `.cache` remains the shared PlatformIO build cache. Do not delete `.pio` or `.cache` between normal builds.

These changes are intended to make warm/incremental builds materially faster. A first build in a new folder still has to compile the ESP-IDF/Arduino cores and cannot be made instant.
