ASAP Agent Guide

Scope: This file applies to the entire repository. Agents must follow the rules below for any file they touch under this repo. It encodes conventions, build and test workflows, and MCU constraints for STM32F103C8T6 targets.

Goals
- Maintain a single codebase for STM32 targets and a native host build used for tests and UI snapshots.
- Keep the native path feature‑parity with the embedded path while allowing richer tooling on host.
- Preserve UI/UX contracts (menu mapping, display layouts) using snapshot tests.

Project Layout
- `src/` — device entry points (`main_*.cpp`).
- `lib/` — shared modules (display, UI, input, RF when added).
- `test/` — Unity unit tests and snapshot flow for the native target.
- `platformio.ini` — multi‑environment config (`detector`, `beacon`, `artifact`, `anomaly`, `native`).

Build & Test
- Native tests and snapshots (preferred for fast iteration):
  - Run: `pio test -e native`
  - Output: PGM snapshots in `snapshots/` (created automatically by tests).
  - Treat snapshots as a contract when modifying UI rendering; update tests intentionally when UI changes.
  - Snapshot naming: a single global counter across tests with action labels
    (e.g., `000_longpress.pgm`, `001_down.pgm`, `002_click.pgm`) to reflect the
    action sequence. See `test/test_main.cpp` `SaveActionSnapshot` helper.
- Device builds (STM32 Blue Pill):
  - Detector: `pio run -e detector`
  - Other roles: `pio run -e beacon|artifact|anomaly`
  - Upload: `pio run -e <env> -t upload`

Agent Permissions & Expectations
- Agents may proactively run `pio test -e native` whenever relevant to verify changes and refresh snapshots.
- Prefer minimal, surgical diffs. Do not modify third‑party code (e.g., anything under `lib/Unity/`).
- Keep native and embedded code paths in sync; test both where possible.

- After any major code modification or feature/strategy change, propose relevant updates to `PROJECT_CONTEXT.md` (human-facing) and `AGENTS.md` (machine-facing) to keep both sources of truth in sync.

Coding Conventions
- Language level: C++17.
- Brace style: Allman (opening brace on a new line) for namespaces, classes/structs/enums, functions, and control blocks.
- Indentation: 2 spaces; no tabs.
- Line length: strive for ≤100 columns where practical.
- Headers: `#pragma once` in project headers.
- Naming:
  - Types: `PascalCase` (`DetectorDisplay`, `UIController`).
  - Functions/methods/variables: `camelCase`.
  - Constants/macros: `kCamelCase` for constants (`kDisplayWidth`), UPPER_SNAKE for macros.
- Includes:
  - Prefer local includes with angle brackets using logical include paths (`<asap/display/DetectorDisplay.h>`).
  - Keep ARDUINO vs native includes behind `#ifdef ARDUINO` guards as established.

Display & UI Contracts
- Frame system: Use `DisplayFrame` factories in `lib/asap_display` to compose pages. Hardware rendering (U8g2) and native renderer must produce consistent visual structure.
- Menu/input mapping (Detector):
  - First‑action gating: first interaction must be a ≥1000 ms long‑press on the center button to enter the menu.
  - Root: ANOMALY, TRACKING, CONFIG. Up/Down to move. Right/Click to enter.
  - ANOMALY page: Left = back; Right/Click = set Main Anomaly and exit.
  - TRACKING page: Up = back; Left/Right = change Track ID; Click = set Main Tracking and exit.
  - CONFIG page: placeholder; Left = back.
- HUDs:
  - Anomaly: 15 px high full‑width progress bar near bottom.
  - Tracking: “TRACK <ID>” and “RSSI <avg>dBm” with EMA smoothing.
- Native glyph table:
  - The native renderer’s 5×7 `kGlyphTable` is only a development aid. When adding UI text that contains unsupported characters, extend `kGlyphTable` accordingly rather than restricting copy.

- UI navigation graph:
  - The page arborescence and transitions are declared in `lib/asap_ui/src/asap/ui/UIController.h` via a `PageNode` table (`kPages`).
  - Up/Down scrolling updates `selectedIndex_` through the page’s action hook using the graph’s `childCount` (no hard‑coded item counts).
  - Confirmation/back behavior is defined declaratively: confirm mask + `EnterSelectedChild` or `GoToTarget`, and a single `backAction` per page.
  - To add/modify pages: add a render hook and optional action hook in `UIController.cpp`, then wire the node in `kPages` (header‑only change for structure).
  - Config submenu is a paged list under `MenuConfig` with items: Invert X Joystick, Invert Y Joystick, Rotate Display, RSSI Calibration (placeholder), Version Display. Each item has its own render/action hook; toggles happen on Right/Click and display ON/OFF.
  - Selection policy: when entering a menu/list page, selection resets to the
    first item to keep navigation deterministic.

Display Assets (XBM Icons)
- Anomaly HUD icons (radiation, fire, biohazard, psy) use 1-bit XBM bitmaps on hardware for crisp rendering.
- Asset location: `lib/asap_display/src/asap/display/assets/AnomalyIcons.h` (30×30 assets, sourced from `pics/`).
- Hardware path draws via `u8g2.drawXBMP` centered on indicator; native mock blits the same XBM in software for snapshots.
- Preferred flow: keep source SVG/PNGs in `pics/` and convert to XBM (e.g., ImageMagick) before embedding.

Anomaly HUD Details
- Icons: 30×30 XBM, ring radius 18, roman label baseline `cy + radius + 13`.
- Arc: single-pass section (no base ring), constant thickness, begins at north (top) and sweeps clockwise.
- Control variables (in `UIController`):
  - `setAnomalyExposure(rad, therm, chem, psy)` for arc progress 0..100%.
  - `setAnomalyStage(rad, therm, chem, psy)` for stages 0..3 (rendered as -, I, II, III).

Persistence Roadmap
- Configuration toggles (invert X/Y, rotation, future RSSI calibration constants) are currently volatile. Implement a global, coherent persistence layer (e.g., a small key/value store or fixed struct) to save and load preferences across boots for both embedded and native builds. Coordinate a single API surface (e.g., `settings::load/save`) so all modules use one mechanism. Load settings before UI init so inversion/rotation take effect from the first tick.

Implemented Toggles
- Joystick inversion (X/Y) is applied at the input layer in `UIController::onTick`.
- Display rotation (180°): `DetectorDisplay::setRotation180` uses U8g2 rotation on embedded and software coordinate flip on native.

MCU Constraints (STM32F103C8T6)
- Keep embedded code feasible for 64 KB flash / 20 KB RAM class devices.
- Use the existing `#ifdef ARDUINO` split:
  - Embedded path: avoid heavy STL (no `<string>`, `<vector>`, `<fstream>` in ARDUINO code). Favor fixed buffers (`DisplayLine::kMaxLineLength`), `std::array`, and stack allocations.
  - Native path: richer STL allowed for testing and snapshots.
- Avoid exceptions and RTTI in embedded paths; do not rely on iostreams. Use `SerialUSB` for logs when needed.
- Prefer non‑allocating patterns in embedded code; no dynamic memory in hot paths.
- Stick to U8g2 for OLED and Arduino SPI/GPIO APIs; avoid introducing platform‑specific dependencies.

When Adding or Modifying Code
- Keep ARDUINO/native parity: update both code paths when behavior changes.
- Update or add Unity tests under `test/` and refresh snapshots via `pio test -e native`.
- Maintain Allman style and the naming/indentation rules above.
- Do not edit third‑party resources in `lib/Unity` or `.pio` outputs.

Verification Checklist (Quick)
- `pio test -e native` passes and snapshots updated intentionally.
- For embedded changes to the Detector: build `pio run -e detector`.
- No violations of MCU constraints (no heavy STL, exceptions, or iostreams in ARDUINO code).
