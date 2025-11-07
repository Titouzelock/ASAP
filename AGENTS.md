ASAP Agent Guide

Scope: This file applies to the entire repository. Agents must follow the rules below for any file they touch under this repo. It encodes conventions, build and test workflows, and MCU constraints for STM32F103C8T6 targets.

Goals
- Maintain a single codebase for STM32 targets and a native host build used for tests and UI snapshots.
- Keep native feature parity with the embedded path while allowing richer tooling on host.
- Preserve UI/UX contracts (menu mapping, display layouts) using snapshot tests.

Project Layout
- `src/` - device entry points (`main_*.cpp`).
- `lib/` - shared modules (display, UI, input, RF when added).
- `test/` - Unity unit tests and snapshot flow for the native target.
- `platformio.ini` - multi-environment config (`detector`, `beacon`, `artifact`, `anomaly`, `native`).

Build & Test
- Device builds (STM32 Blue Pill):
  - Detector: `pio run -e detector`
  - Other roles: `pio run -e beacon|artifact|anomaly`
  - Upload: `pio run -e <env> -t upload`

Agent Permissions & Expectations
- Prefer minimal, surgical diffs. Do not modify third-party code (e.g., anything under `lib/Unity/`).
- Keep native and embedded code paths in sync; test both where possible.
- After any major code modification or feature/strategy change, propose relevant updates to `PROJECT_CONTEXT.md` (human-facing) and `AGENTS.md` (machine-facing) to keep both sources of truth in sync.

Coding Conventions
- Language level: C++17.
- Brace style: Allman (opening brace on a new line) for namespaces, classes/structs/enums, functions, and control blocks.
- Indentation: 2 spaces; no tabs.
- Line length: strive for ≈100 columns where practical.
- Headers: `#pragma once` in project headers.
- Naming:
  - Types: `PascalCase` (`DetectorDisplay`, `UIController`).
  - Functions/methods/variables: `camelCase`.
  - Constants/macros: `kCamelCase` for constants (`kDisplayWidth`), UPPER_SNAKE for macros.
- Includes:
  - Prefer logical include paths (`<asap/display/DetectorDisplay.h>`).
  - Keep ARDUINO vs native specifics confined to the two thin wrappers (DetectorDisplay/NativeDisplay). Shared code (DisplayTypes/Renderer) is platform-neutral.

Persistence Roadmap
- Configuration toggles (invert X/Y, rotation, future RSSI calibration constants) are currently volatile. Implement a global, coherent persistence layer (e.g., a small key/value store or fixed struct) to save and load preferences across boots for both embedded and native builds. Coordinate a single API surface (e.g., `settings::load/save`) so all modules use one mechanism. Load settings before UI init so inversion/rotation take effect from the first tick.

MCU Constraints (STM32F103C8T6)
- Keep embedded code feasible for 64 KB flash / 20 KB RAM class devices.
- Use the existing `#ifdef ARDUINO` split:
  - Embedded path: avoid heavy STL (no `<string>`, `<vector>`, `<fstream>` in ARDUINO code). Favor fixed buffers (`DisplayLine::kMaxLineLength`), `std::array`, and stack allocations.
  - Native path: richer STL allowed for testing and snapshots.
- Avoid exceptions and RTTI in embedded paths; do not rely on iostreams. Use `SerialUSB` for logs when needed.
- Prefer non-allocating patterns in embedded code; no dynamic memory in hot paths.
- Stick to U8g2 for OLED and Arduino SPI/GPIO APIs; avoid introducing platform-specific dependencies.

When Adding or Modifying Code
- Keep ARDUINO/native parity: update both code paths when behavior changes.
- Maintain Allman style and the naming/indentation rules above.
- Do not edit third-party resources in `lib/Unity` or `.pio` outputs.

Verification Checklist (Quick)
- For embedded changes to the Detector: build `pio run -e detector`.
- No violations of MCU constraints (no heavy STL, exceptions, or iostreams in ARDUINO code).
