ASAP Display Icon Assets (XBM)

Purpose
- Store 1‑bit XBM icons used by the anomaly HUD (radiation, fire, biohazard, psy).
- Target size: 30×30 pixels, centered, high‑contrast, minimal detail.

Where they are used
- Hardware: drawn via `u8g2.drawXBMP()` from arrays in `lib/asap_display/src/asap/display/assets/AnomalyIcons.h`.
- Native mock: blitted with the same data for snapshots.

Naming and mapping
- Pics → Code arrays:
  - `pics/Radiation_warning_symbol2.svg.xbm` → `kIconRadiation30x30`
  - `pics/fire-svgrepo-com.xbm` → `kIconFire30x30`
  - `pics/Biohazard_symbol.svg.xbm` → `kIconBiohazard30x30`
  - `pics/brain-and-head-svgrepo-com.xbm` → `kIconPsy30x30`
- Arrays live in: `lib/asap_display/src/asap/display/assets/AnomalyIcons.h`

Conversion (SVG/PNG → XBM)
- Requirements: ImageMagick (v7+ preferred; `magick` or fallback `convert`)
- One‑shot command:
  - `magick input.svg -resize 30x30 -gravity center -background none -extent 30x30 -colorspace Gray -threshold 55% -monochrome xbm:output.xbm`
- Batch script (recommended):
  - `bash pics/convert_xbm.sh` (converts all *.svg/*.png in `pics/`)
  - Environment overrides: `SIZE=30x30 THRESHOLD=60% bash pics/convert_xbm.sh`
- Tips:
  - Ensure the icon is visually centered and fits a 30×30 square without clipping.
  - Tune threshold (50–65%) for crisp, bold strokes; avoid anti‑aliasing.

Bit order expectations
- Our blitters assume standard XBM bit order (LSB‑first per byte). The provided examples render correctly.
- If an icon appears mirrored or corrupted in native snapshots, re‑export and retest.

How to update code arrays
- Open the new `.xbm`, copy the data between `{` and `}` into the corresponding array in `AnomalyIcons.h`.
- Run `pio test -e native` to review updated snapshots.

Design guidance
- Keep strokes thick and shapes simple for legibility on 64 px‑tall displays.
- Avoid thin gaps that may disappear at 1‑bit.
