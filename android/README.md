# PlayCompanion Android MVP

This is the first Android client for the CrossPoint/PlayCompanion Wi-Fi server.

The current milestone implements connection and browsing through the existing
`GET /api/files?path=...` endpoint. The default address is the X4 Pro access
point address (`192.168.4.1`); when the reader is connected to a normal Wi-Fi
network, enter the address shown by the firmware.

Next milestones: UDP discovery, upload/download, and file operations.

Open this directory in Android Studio and run the `app` configuration.
