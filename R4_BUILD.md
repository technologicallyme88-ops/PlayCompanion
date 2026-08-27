# X4 Pro Play+Companion r4

r4 is based on the known-good r3 X4 Pro build.

## Companion changes

- Added **Noodle** (`kind: dog`) as a native reading companion.
- Removed **Lumen** (`moth`) and **Sprig** (`sprout`) from the generated companion table and settings picker.
- Kept Sophocles (fox), Vellum (ghost), and Octavo (robot).
- Noodle uses four custom poses derived from the supplied dog photograph/character artwork: thriving, content, peckish, and neglected.
- Added Noodle-specific mood and milestone quotes.
- Noodle is the default companion for fresh settings.
- Legacy companion migration: old Lumen id 3 maps naturally to Noodle; stale old Sprig id 4 is migrated to Noodle at startup.

## Firmware identity

- Default environment: `x4pro_r4`
- Firmware version: `1.5.0-x4pro-playcompanion-r4`
- Packaged output: `dist/x4pro-playcompanion-r4.bin`

## Build

Open the project in VS Code / PlatformIO and run Build, or:

    platformio run -e x4pro_r4

The packaged firmware and SHA-256 will be placed in `dist/` after a successful build.
