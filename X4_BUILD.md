# Xteink X4 build target (r8)

This source tree now supports both Xteink hardware families from the same codebase.

## Original X4 (ESP32-C3)

Build environment:

```text
x4_r8
```

Output:

```text
dist/x4-playcompanion-r8.bin
```

Hardware profile used: `FREEINK_DEVICE_X4=1` on `esp32-c3-devkitm-1`.
No X4 Pro PSRAM, capacitive touch, SDMMC, warm-light, or ESP32-S3 flags are enabled.

Because X4 has no touch panel, multi-page Companion messages use the physical front left/right buttons for previous/next page while the side up/down buttons remain available for Home-menu navigation.

## X4 Pro (ESP32-S3)

Build environment remains:

```text
x4pro_r8
```

Output:

```text
dist/x4pro-playcompanion-r8.bin
```

Do not flash an X4 Pro binary to an X4, or an X4 binary to an X4 Pro. They use different MCU families.
