# X4 Pro Play+Companion r3

r3 is a UI-polish release based on the known-good r2 hardware/game configuration.

## Companion speech bubbles

Companion dialogue no longer uses the generic `wrappedText(..., maxLines)` path, whose final line is deliberately shortened with an ellipsis. Home-screen Companion bubbles now wrap the complete quote. The renderer keeps the normal UI font where it fits and automatically drops to the small UI font when a constrained theme/strip needs more lines.

This change applies to the roomy horizontal Companion layout, compact horizontal layout, and narrow column layout. No Companion quote is intentionally replaced with `...` by these paths.

## Build

The default environment is `x4pro_r3`:

    platformio run -e x4pro_r3

The packaged output is:

    dist/x4pro-playcompanion-r3.bin

A logging build is available as `x4pro_r3_debug`.
