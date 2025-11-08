Display Guide

Display & UI Contracts
- Frame system: Use `DisplayFrame` factories in `lib/asap_display` to compose pages. Hardware and native both render via the same U8g2-based helpers in `DisplayRenderer.*`.

Display Assets (XBM Icons)
- Anomaly HUD icons (radiation, fire, biohazard, psy) are 1-bit XBM bitmaps.
- Asset location: `lib/asap_display/src/asap/display/assets/AnomalyIcons.h` (30x30, sourced from `pics/`).
- Both paths draw via `u8g2.drawXBMP` centered on the indicator.
- Preferred flow: keep source SVG/PNGs in `pics/` and convert to XBM before embedding.

Anomaly HUD Details (Current Contract)
- Geometry
  - Icons: 30x30 XBM.
  - Indicator centers (top-aligned layout): (32,23), (96,23), (160,23), (224,23).
  - Arc: ring radius 21 px, thickness 3 px; arc begins at north and sweeps clockwise.
  - Roman numerals baseline is fixed at y=57 for all indicators (constant), independent of icon/circle vertical shifts. See `kRomanBaselineY` in `DetectorDisplay.cpp`.
- Rendering
  - Both paths: U8g2 draw path (`DrawArcU8g2`, `drawXBMP`) for consistent visuals.

Implemented Toggles
- Display rotation (180°): `DetectorDisplay::setRotation180` uses U8g2 rotation on embedded; native mirrors rotation via U8g2 as well.

