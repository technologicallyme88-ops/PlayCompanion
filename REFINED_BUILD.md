# X4 Pro Play + Companion — Refined Build r1

This tree is the working X4 Pro merge of CrossPoint 1.5.0, CrossPlay games, Reader Companion, and FreeInk SDK 310ec61506fc915836db7799a2e7f4fc135a570d.

## What r1 changes

- Defaults PlatformIO to `x4pro_refined` so a generic Build cannot accidentally target the X3/X4 ESP32-C3 environment.
- Includes the `HalClock::getUtcDateTime()` compatibility API required by Companion day tracking.
- Uses the visible firmware identity `1.5.0-x4pro-playcompanion-r1`.
- Keeps the proven X4 Pro ESP32-S3/PSRAM/SDMMC/frontlight/touch path unchanged.
- Compiles serial logging out of the daily-driver build to save flash.
- Retains `x4pro_refined_debug` with serial logging for troubleshooting.
- After a successful build, copies the binary and SHA-256 into `dist/` with a descriptive filename.

## Build the daily-driver firmware

Use PlatformIO **Build**, or explicitly:

    platformio run -e x4pro_refined

The finished image is copied to:

    dist/x4pro-playcompanion-r1.bin

For SD-card updating, copy that file to the SD-card root and rename the copy to `firmware.bin` if the updater requires that exact filename.

## Debug build

    platformio run -e x4pro_refined_debug

Output:

    dist/x4pro-playcompanion-r1-debug.bin

Use the debug build only when serial logs are needed.

## Known-good baseline

The pre-refinement X4 Pro merge compiled successfully and booted on physical X4 Pro hardware with both Companion and the Games shelf visible. Keep that known-good binary as a recovery/reference image while testing r1.
