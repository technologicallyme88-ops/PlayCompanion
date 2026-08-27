# Play+Companion r11

r11 adds two development/release changes on top of r10:

1. **Parallel Build All** for X3, X4 and X4 Pro.
2. **Lincoln**, a new Labrador companion based on the supplied photo.

## Targets

- `x3_r11` — Xteink X3 / ESP32-C3 (compiles both X3+X4 C3 profiles so FreeInk can detect/select the X3 hardware at runtime)
- `x4_r11` — Xteink X4 / ESP32-C3
- `x4pro_r11` — Xteink X4 Pro / ESP32-S3
- `build_all_r11` — launches all three builds concurrently

Debug variants are available as `x3_r11_debug`, `x4_r11_debug`, and `x4pro_r11_debug`.

## Build all

In PlatformIO use **Project Tasks > build_all_r11 > Build**. Three independent PlatformIO processes are launched at the same time. Their detailed output is written to:

- `build-x3_r11.log`
- `build-x4_r11.log`
- `build-x4pro_r11.log`

Successful release files are placed in `dist/` as:

- `x3-playcompanion-r11.bin`
- `x4-playcompanion-r11.bin`
- `x4pro-playcompanion-r11.bin`

Do not flash a binary intended for another device model.

## Companion roster

Sophocles, Vellum, Octavo, Noodle, Lincoln. Noodle remains the default for existing/new settings; Lincoln is appended as ID 4 so existing IDs remain stable.
