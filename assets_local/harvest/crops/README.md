# Harvest crop art

Copy the four `stage-*.bmp` files to this directory on the reader SD card:

`/.crosspoint/harvest/crops/`

The Harvest theme first looks for crop- and branch-specific artwork named
`<crop>-<stage>-<branch>.bmp`, for example `01-4-scholar.bmp`. When that file is
absent it uses the shared `stage-<stage>.bmp` asset. If neither exists, the
firmware falls back to its built-in procedural crop drawing.

Crop IDs run from `01` through `08`, stages from `1` through `4`, and branch
names are `default`, `scholar`, and `wild`.
