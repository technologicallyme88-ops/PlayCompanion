# X4 Pro Play+Companion r2

This build is based on the known-good r1 merge and keeps the X4 Pro hardware target and Companion integration intact.

## Games retained

- Knucklebones
- Murdle
- Minesweeper
- Solitaire
- D&Diagrams
- Connections

The other CrossPlay game source modules have been removed from this package so they cannot be compiled into the firmware. The Apps shelf (Study, Hacker News, XKCD) is unchanged.

## Boot identity

The boot splash now displays `X4 PRO PLAY+COMPANION`, with `1.5.0-x4pro-playcompanion-r2` as the version string at the bottom.

## Build

The default PlatformIO environment is `x4pro_r2`. Build from VS Code using PlatformIO > Build, or run:

    platformio run -e x4pro_r2

A successful build is copied to:

    dist/x4pro-playcompanion-r2.bin

A debug environment is also available as `x4pro_r2_debug`.

## Apps shelf removed

This revision removes the Apps shelf and its Study, Hacker News, and XKCD source modules from the firmware. Home exposes only the Games shelf from CrossPlay, containing the six selected games. Companion remains integrated separately.
