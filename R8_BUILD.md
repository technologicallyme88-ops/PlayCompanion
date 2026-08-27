# Play+Companion r8 — build-system polish

r8 intentionally keeps the known-good r7 on-device feature set and focuses on making future firmware work safer and easier.

## What changed

- Default stable environment is `x4pro_r8`; `x4pro_r8_debug` keeps serial logging for troubleshooting.
- JPEGDEC progressive-JPEG fixes are now applied by an idempotent in-place Python patcher. It does not call `git apply`, does not unlink `jpeg.inl`, and performs no write after the fix is present. This is designed to avoid the Windows file-lock failure seen in earlier builds.
- Companion PNGs are validated during every build. Missing, malformed, unsupported, or blank pose images fail early instead of generating a bad firmware sprite.
- `build_artifacts/companion-preview.png` is generated from the exact same 1-bit conversion path used by the firmware. It is a contact sheet of every companion/mood as it will actually compile.
- The release packager verifies the ESP image magic byte, refuses non-r8 environments, reports app-partition headroom, and applies a high-water safety limit.
- Every successful build produces `dist/RELEASE.txt`, `dist/release-manifest.json`, SHA-256, and a copy of the Companion preview alongside the firmware.
- The distributed project contains a tiny local Git repository with an empty r8 baseline commit. No GitHub account is required; it simply gives ESP-IDF/Git tooling a valid repository and provides a local revision identifier.

## Stable build

Open this folder in VS Code and use PlatformIO Build, or:

    platformio run -e x4pro_r8

Output:

    dist/x4pro-playcompanion-r8.bin

## Debug build

    platformio run -e x4pro_r8_debug

The debug build includes serial logging and is packaged separately so it cannot overwrite the stable binary.

## Editing Companion artwork

Edit PNGs under `assets/companions/<name>/`. Each companion requires:

- `thriving.png`
- `content.png`
- `peckish.png`
- `neglected.png`

The build validates them and refreshes `build_artifacts/companion-preview.png` automatically.
