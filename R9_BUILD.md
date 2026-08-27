# Play+Companion r9

r9 adds a one-click dual-device build and refreshes all four Companion sprites from the supplied grid artwork.

## One-click Build All

In PlatformIO Project Tasks, expand `build_all_r9` and click **Build**. It builds `x4_r9` first and `x4pro_r9` second.

Outputs:

- `dist/x4-playcompanion-r9.bin`
- `dist/x4pro-playcompanion-r9.bin`

You can still build either target individually.

## Updated Companion grids

The supplied art grids were applied to Sophocles, Vellum, Octavo, and Noodle. Existing validated dialogue/quote sections were retained. The updated grids were also converted with the firmware's exact dither rules into `assets/companions/<id>/*.png`, so r7/r8's editable-PNG workflow remains available and the compiled r9 sprites match the supplied grid artwork.

Order remains: Sophocles, Vellum, Octavo, Noodle.

## Per-device metadata

Build All preserves separate release metadata for each target:

- `dist/x4-RELEASE.txt` / `dist/x4-release-manifest.json`
- `dist/x4pro-RELEASE.txt` / `dist/x4pro-release-manifest.json`
