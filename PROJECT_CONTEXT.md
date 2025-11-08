# Airsoft Stalker Anomalous Project (ASAP)

## Overview

**ASAP** is a modular embedded system designed for immersive airsoft and roleplay events inspired by the S.T.A.L.K.E.R. universe.

The project consists of a family of custom STM32-based devices communicating over low-power radio links (CC1101), each serving a specific role within the game ecosystem:

- **Detector** � Handheld player device with an OLED display and haptic/LED feedback. Detects signals from anomalies and artifacts.
- **Beacon** � Stationary transmitter marking anomaly zones or invisible hazards.
- **Artifact** � Portable or hidden objects emitting special RF signatures that can be detected by players.
- **Anomaly** � Environmental device simulating unstable zones with random emission bursts.
- **Native** � Host environment used for simulation and automated unit testing (PlatformIO + Unity) with U8g2 full-buffer rendering for snapshots.

---

## Technical Stack

- **MCU Family:** STM32F103C8T6 ("Blue Pill")
- **Framework:** Arduino for STM32 (via PlatformIO)
- **Display:** SSD1322 OLED (U8g2 library)
- **Radio:** CC1101 (433 MHz)
- **Build System:** PlatformIO (multi-environment)
- **Testing:** Unity (unit tests on native environment)

---

## Recent Changes (Nov 2025)

- Declarative UI navigation graph (`UIController`): pages and transitions defined via a `PageNode` table in `lib/asap_ui/src/asap/ui/UIController.h`. Back/confirm behavior is encoded per-page; selection logic derives from child counts (no magic numbers).
- Config submenu built out under `MenuConfig`: Invert X Joystick, Invert Y Joystick, Rotate Display, RSSI Calibration (placeholder), Version Display. Each item exposes a render/action hook.
- Player Data persistence module added (`lib/asap_player/`): packed structs with CRC-16/CCITT-FALSE, versioning, and parity between embedded and native backends.
- Native UI snapshot testing: `pio test -e native` exports PGM snapshots to `snapshots/`, used as a visual contract for UI changes. The test runner cleans previous `.pgm` files and resets numbering per run.

---

## Build & Test

- Device builds (STM32 Blue Pill):
  - Detector: `pio run -e detector`
  - Beacon/Artifact/Anomaly: `pio run -e beacon|artifact|anomaly`
  - Upload: `pio run -e <env> -t upload`
- Native host tests and snapshots:
  - Run: `pio test -e native`
  - Output: PGM snapshots under `snapshots/` for boot, heartbeat, anomaly HUD, and end-to-end menu navigation.

---

## Modules

- Display/UI
  - Display frames built in `lib/asap_display`, rendered via U8g2 on both embedded and native targets.
  - UI state and navigation live in `lib/asap_ui/src/asap/ui/`, with page-specific render/action hooks.
- Input
  - Debounced joystick actions mapped to high-level `JoyAction`; first interaction requires a 1000 ms long-press on center to enter the menu.
- Player Data Persistence (`lib/asap_player`)
  - API: `load(state)`, `save(state)`, `resetSession(state)`, and `loadPersistent/savePersistent`.
  - Embedded backend uses HAL Flash at the reserved page; native backend writes to a file for tests.
  - UART framing helpers (`UARTFrame.*`) support import/export workflows.

---

## UI Contracts (Summary)

- Root menu: ANOMALY, TRACKING, PLAYER DATA, CONFIG. Up/Down scroll; Right/Click confirms.
- Main pages:
  - Anomaly HUD: four indicators with arc progress (0..100%) and roman stage labels (-, I, II, III); 15 px bottom progress bar retained for legacy strength display.
  - Tracking HUD: TRACK <ID> and smoothed RSSI readout.
- Player Data page: read-only; 3-line window with paged scrolling, hides the top-right MENU tag while active.


## Architectural Goals

1. **Modularity** � Each device shares a common codebase with specific build environments (`platformio.ini`).
2. **Parity** � Shared U8g2 renderer for both embedded and native ensures identical layouts and behavior.
3. **Power Efficiency** � Keep MCU memory/CPU budgets under control for long runtime.
4. **Scalability** � Architecture meant to scale to many devices.
5. **Debuggability** � UART logs, snapshots, and heartbeat where applicable.

---

## Development Environment

- **/src** � Firmware entry points (`main_detector.cpp`, etc.)
- **/lib** � Shared modules (display, UI, input, RF)
- **/test** � Unit tests and snapshot generation on host
- **platformio.ini** � Multi-device environment configuration

### Display Architecture (Unified)
- **Types:** `lib/asap_display/src/asap/display/DisplayTypes.h` (platform-neutral `DisplayFrame`, `DisplayLine`, `FrameKind`, factories)
- **Renderer:** `lib/asap_display/src/asap/display/DisplayRenderer.*` � U8g2-based draw helpers used by both targets
- **Embedded wrapper:** `DetectorDisplay.*` � owns SSD1322 U8g2 and calls shared renderer
- **Native wrapper:** `NativeDisplay.*` � owns base U8G2 configured for SSD1322 full-buffer
- **Snapshots:** PGM `P5` (MaxVal 15, 4-bit), decoded from U8g2�s vertical-top buffer

---

## Current Focus

- Detector UI skeleton + shared renderer verified by snapshots
- Menu system (root: ANOMALY, TRACKING, CONFIG) with long-press gating (=1000 ms)
- Anomaly and Tracking HUDs
- Native snapshot contract (4-bit PGM) for CI/UI reviews

### Detector UI and Menu System
- UI driven by `lib/asap_ui/src/asap/ui/UIController.*` (state machine + declarative graph)
- First long-press (=1000 ms) enters menu; subsequent long-presses open menu
- Root: ANOMALY, TRACKING, CONFIG � Up/Down navigate, Right/Click enter
- CONFIG submenu (Invert X, Invert Y, Rotate Display, RSSI Calibration placeholder, Version)
- Selection resets to the first item when entering a list/menu page

### HUD Contracts
- Anomaly: 15 px tall full-width progress bar
- Tracking: �TRACK <ID>� and �RSSI <avg>dBm� with EMA smoothing
- Anomaly HUD geometry (indicators): centers (32,23), (96,23), (160,23), (224,23); arc radius 21 px, thickness 3 px; roman baseline y=57

---

## Persistence (Roadmap)
- Joystick inversion, rotation, and future calibration values should be persisted via a shared `settings::load/save` API with both embedded and native backends. Load settings before UI init.

---

## Build & Test
- Native snapshots: `pio test -e native` generates 4-bit PGM snapshots in `snapshots/`
- Embedded detector build: `pio run -e detector`
- Other roles: `pio run -e beacon|artifact|anomaly`

---

## Summary
ASAP is a distributed interactive system; rendering parity and snapshot tests ensure a consistent player experience. The unified U8g2 renderer and 4-bit snapshot pipeline keep native and hardware visuals in lockstep.
