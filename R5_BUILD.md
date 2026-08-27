# X4 Pro Play+Companion r5

r5 is based on the known-good r4 Noodle build.

## Companion UI changes

- Noodle's upright sprite poses are rebalanced to occupy more of the existing 34x30 sprite canvas. This makes him visibly larger without changing the sprite table format or increasing RAM usage.
- Long Companion speech is now genuinely paginated instead of merely wrapped into a clipped bubble.
- A `page / pages` indicator appears in the bubble only when more than one page is required.
- On the Home screen, swipe **left** for the next Companion page and **right** for the previous page. Up/down continue to navigate the Home menu normally.
- Pagination is implemented in roomy, compact, and column Companion layouts.
- r4's Noodle companion roster, six-game shelf, no-Apps configuration, X4 Pro hardware target, and RTC fixes are preserved.

## Firmware identity

- Default environment: `x4pro_r5`
- Firmware version: `1.5.0-x4pro-playcompanion-r5`
- Packaged output: `dist/x4pro-playcompanion-r5.bin`

## Build

Open the project in VS Code / PlatformIO and run Build, or:

    platformio run -e x4pro_r5

The packaged firmware and SHA-256 will be placed in `dist/` after a successful build.
