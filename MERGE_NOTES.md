# X4 Pro CrossPlay + Reader Companion merge

This tree merges:
- CrossPlay v1.3.3 game/app shelf and X4 Pro ESP32-S3 target
- CrossPoint Reader Companion v1.0.2 companion feature
- FreeInk SDK commit 310ec61506fc915836db7799a2e7f4fc135a570d

## Target
Build only the X4 Pro environment:

    pio run -e x4pro

The expected application image is normally written under `.pio/build/x4pro/firmware.bin`.

## Important
This source merge was prepared in an environment without PlatformIO/ESP32 build packages and therefore has NOT been firmware-compiled or hardware-tested here. Do not rename an arbitrary .bin and flash it as this build.

The CrossPlay X4 Pro hardware configuration, ESP32-S3 target, PSRAM/flash configuration, touch hooks, shelf navigation, Minesweeper touch suppression, and CrossPlay OTA channel are retained. Reader Companion state, settings, sprites, home-screen rendering, and reader-session tracking were added around those seams.

Keep a known-good X4 Pro recovery firmware before testing a compiled result.
