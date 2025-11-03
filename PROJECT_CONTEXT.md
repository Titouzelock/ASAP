# Airsoft Stalker Anomalous Project (ASAP)

## Overview

**ASAP** is a modular embedded system designed for immersive airsoft and roleplay events inspired by the *S.T.A.L.K.E.R.* universe.

The project consists of a family of custom STM32-based devices communicating over low-power radio links (CC1101), each serving a specific role within the game ecosystem:

- **Detector** – Handheld player device with an OLED display and haptic/LED feedback. Detects signals from anomalies and artifacts.
- **Beacon** – Stationary transmitter marking anomaly zones or invisible hazards.
- **Artifact** – Portable or hidden objects emitting special RF signatures that can be detected by players.
- **Anomaly** – Environmental device simulating unstable zones with random emission bursts.
- **Native** – PC-based environment used for simulation and automated unit testing (via PlatformIO + Unity).

The project aims to **blend technical engineering with immersive gameplay**, building a wireless ecosystem that behaves like a “living” field environment.

---

## Technical Stack

- **MCU Family:** STM32F103C8T6 ("Blue Pill")
- **Framework:** Arduino for STM32 (via PlatformIO)
- **Display:** SSD1322 OLED (U8g2 library)
- **Radio:** CC1101 (433 MHz) modules for bidirectional low-rate communication
- **Build System:** PlatformIO (multi-environment)
- **Testing:** Unity (unit tests on native environment)
- **CI/CD:** GitHub Actions (auto build + test verification)

---

## Architectural Goals

1. **Modularity** – Each device shares a common codebase with specific build environments (`platformio.ini` manages this separation).
2. **Robust Communication** – Custom RF protocol designed for field reliability: anti-collision, dwell-time hysteresis, RSSI smoothing.
3. **Power Efficiency** – Designed for long runtime on small battery packs (AAA or Li-ion), using aggressive sleep modes and short packet bursts.
4. **Scalability** – Architecture meant to scale to 20+ simultaneous devices in one field without interference.
5. **Debuggability** – Every firmware includes UART debug logs, optional screen display, and heartbeat LED.

---

## Gameplay Context (for design reasoning)

- The *Detector* should feel alive: scanning pulses, responsive beeps, flickering OLED graphics.
- *Beacons* and *Anomalies* behave unpredictably to create tension; RF emission patterns are randomized.
- *Artifacts* are the "treasures" — emitting rare identifiable signals.
- Players move through zones with realistic sensor feedback, not just “on/off” detection.

From a code standpoint, **this means timing, signal pattern generation, and state persistence** matter as much as RF transmission itself.

---

## Development Environment

- **/src** → Firmware entry points (main_detector.cpp, etc.)
- **/lib** → Shared modules (RF protocol, display, sensors)
- **/test** → Unit tests for logic simulation on PC
- **/.github/workflows** → Continuous Integration scripts
- **platformio.ini** → Multi-device environment configuration

---

## Current Focus

Initial phase (v0.1):
- Build and validate the **Detector** firmware skeleton:
  - SSD1322 OLED display initialization (via U8g2)
  - Boot animation + heartbeat LED
  - Serial logging + status codes
- Unit test verification in `env:native`
- Prepare modular RF layer stubs for integration later

### Detector UI and Menu System (added)

- UI driven by a small state machine (`lib/asap_ui/src/asap/ui/UIController.*`).
  - First-action gating: the first user interaction must be a 1000 ms long-press on the center button to enter the menu.
  - Subsequent long-presses also open the menu.
- Menu structure (shows a top-right "MENU" tag):
  - Root: ANOMALY, TRACKING, CONFIG. Up/Down to move, Right/Click to enter.
  - ANOMALY page: Left = back, Right/Click = set main display mode to Anomaly and exit.
  - TRACKING page: Up = back, Left/Right = change Track ID, Click = set main display mode to Tracking and exit.
  - CONFIG page: placeholder scaffold for future settings.
- Main display modes:
  - Anomaly HUD: 15 px tall, full-width, bordered progress bar at the bottom (0–100%).
  - Tracking HUD: "TRACK <ID>" and "RSSI <avg>dBm" with EMA smoothing.
- Display frame factories encapsulate pages (`DetectorDisplay.cpp`):
  - `makeMenuRootFrame`, `makeMenuAnomalyFrame`, `makeMenuTrackingFrame`
  - `makeAnomalyMainFrame`, `makeTrackingMainFrame`
  - Hardware path draws via U8g2; native path rasterizes to a grayscale buffer and supports snapshots.
- Declarative nav graph:
  - The whole page arborescence and transitions are declared in `UIController.h` via a `PageNode` table. Each page defines parent, children, back action, confirm behavior, and hooks.
  - Up/Down scrolling uses the graph’s `childCount` (no hard-coded counts) through the root menu action hook.
  - Confirm (Right/Click) either enters the selected child or jumps to a target state; back uses a per-page mapping (Left/Up as specified above).
  - Selection policy: entering a list/menu resets selection to the first item.

#### Config Submenu (new)
- `CONFIG` now opens a submenu (scrollable) with these pages:
  - Invert X Joystick — Right/Click toggles X inversion; shows ON/OFF under the title.
  - Invert Y Joystick — Right/Click toggles Y inversion; shows ON/OFF under the title.
  - Rotate Display — Right/Click toggles 180° rotation preference; shows ON/OFF under the title.
  - RSSI Calibration — placeholder page for future integration.
  - Version Display — shows `FW <ASAP_VERSION>`.
- Navigation: Up/Down scroll items (windowed to 3 visible rows), Left backs to root menu; items confirm with Right/Click.
- Notes: toggles are stored in volatile flags inside `UIController` (persistence TBD). Display rotation is a preference flag only at this stage; integration with the hardware/native renderers can be added later.

Persistence (Future Work)
- Preferences such as joystick inversion, screen rotation, and RSSI calibration must be persisted across boots using a single, coherent approach shared by all modules. Define a small settings API and structure, provide an embedded backend (flash/EEPROM) and a native backend (file‑based), and ensure load/save occurs early in boot before UI initialization so behavior applies from first render.

Testing & Snapshots
- Tests use a single global snapshot counter and action-based names (`000_longpress.pgm`, `001_down.pgm`, etc.) to reflect the exact action sequence.
- Assertions check key states (e.g., entering Config list, Rotate page title, ON/OFF state, tracking main page) to auto-validate flow in addition to snapshots.

### Anomaly HUD (indicators)

- Layout (top-aligned): indicator centers are (32,23), (96,23), (160,23), (224,23).
- Arc: single-pass section (no base ring), thickness 3 px, radius 21 px; starts at north and sweeps clockwise.
- Icons:
  - Hardware (SSD1322): 1-bit XBM via U8g2 (`drawXBMP`).
  - Native mock: same XBM blitted in software; arc path uses trig for smooth circles.
  - Assets live in `lib/asap_display/src/asap/display/assets/AnomalyIcons.h` (30�30 XBM, from `pics/`).
- Roman numerals: stages -, I, II, III centered below each indicator on a fixed baseline `kRomanBaselineY = 57` to preserve layout stability.
- UIController API: `setAnomalyExposure(rad, therm, chem, psy)`, `setAnomalyStage(rad, therm, chem, psy)` feed the HUD.
- Snapshots (native env): ordered PGM images are generated by tests to visualize UI navigation.
  - See tests in `test/test_main.cpp`.### Detector Display Notes

- Wiring (SPI 4-wire): `PA5` SCK, `PA7` MOSI, `PA4` CS, `PA3` DC, `PA2` RST, +3V3/GND for power.
- Library scaffold: `lib/asap_display` exposes `DetectorDisplay` (current `FrameKind`s: `Boot`, `Heartbeat`, `Status`).
- Heartbeat page: refreshes every 250 ms with uptime text; activity spinner removed to leave room for copy.
- Native mock: `env:native` reuses the driver to capture frames, validated via Unity tests.
- Snapshot export: native driver rasterises display frames into grayscale PGM snapshots for CI/UI review.
- Future UI: menu/navigation flows will add new frame builders; existing tests keep the rendering contract stable.
- Open items: brightness tuning, asset pipeline for icons, off-hardware verification harness for CI.
- Next integration: hook heartbeat driver to LED/haptics, add error banners when peripherals fail.

---

## Collaboration and AI Use

Codex and ChatGPT are used as **co-developers**:
- To refactor firmware modules cleanly across devices.
- To write maintainable embedded code with safety and portability in mind.
- To generate tests, telemetry parsers, and simulation tools.
- To evolve the communication protocol intelligently (timing, collision avoidance, data structure).

Codex has full context to propose:
- Firmware structure refinements.
- Class designs for modularity.
- Low-level optimizations for power and performance.
- Suggestions for testing, CI, and versioning.

---

## Summary

**ASAP** is not just firmware — it’s a distributed interactive system simulating a dynamic world.  
Every module contributes to immersion, signal realism, and player experience.  
Technical excellence directly enhances narrative tension.

