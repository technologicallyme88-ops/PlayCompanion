# r6 — Noodle aspect-ratio and vertical-space refinement

r6 builds on the known-good r5 firmware.

## Changes

- Restores Noodle's natural, narrower r4 pixel-art proportions instead of widening the source art.
- Adds ink-bound sprite measurement and trimmed rendering. Transparent padding no longer limits Noodle's usable scale.
- The narrow Home companion column can scale Noodle up to 6x while preserving a single integer scale on both axes.
- This uses the previously wasted vertical space below/around the sprite rather than stretching Noodle horizontally.
- Other companions keep the existing renderer and layout behavior.
- r5 paginated speech bubbles and swipe navigation are preserved.

## Build

Default environment: `x4pro_r6`

```powershell
platformio run -e x4pro_r6
```

Output: `dist/x4pro-playcompanion-r6.bin`
