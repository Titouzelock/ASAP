# ASAP Agent Quick Guide

This is the short operational rulebook for every agent in this repo. For product or UX background, read `PROJECT_CONTEXT.md`. Keep the native host build and the STM32F103C8T6 firmware moving together.

## Priorities
- Preserve feature parity between native and `#ifdef ARDUINO` paths.
- Guard UI and display contracts; treat snapshot images as the contract.
- Keep diffs surgical, avoid third-party directories (`lib/Unity`, `.pio`), and refresh shared docs (`PROJECT_CONTEXT.md`, this file) after major behavior updates.

## Core Workflow
- Fast regression loop: `pio test -e native`. Run it for UI or logic changes and accept snapshot diffs only when intentional.
- Firmware sanity: `pio run -e detector` (or the relevant environment) when embedded behavior changes.
- Snapshot naming is `NNN_action.pgm` (global counter + action label). The helper lives in `test/test_main.cpp`.

## Coding Standards
- C++17, Allman braces, 2-space indentation, target <=100 columns.
- Headers start with `#pragma once`.
- Naming: Types `PascalCase`; functions/variables `camelCase`; constants `kCamelCase`; macros `UPPER_SNAKE`.
- Prefer `<asap/...>` include paths and keep Arduino-only headers within the existing `#ifdef ARDUINO` guards.

## Platform Parity & Constraints
- Embedded path: avoid heavy STL headers (`<string>`, `<vector>`, `<fstream>`), heap allocation in hot paths, exceptions, RTTI, and iostreams. Favor fixed buffers and `std::array`.
- Rendering: both paths now use U8g2; hardware streams to the SSD1322, native renders into a 4-bit U8g2 buffer and exports PGM snapshots. Extend fonts or glyph tables when new characters are needed.
- Build UI pages with the `DisplayFrame` factories in `lib/asap_display` so layouts stay in sync across targets.

## Detector UI Contract (Quick Reference)
- First interaction must be a >=1000 ms center-button long-press to open the menu.
- Root menu: ANOMALY, TRACKING, CONFIG. Up/Down moves, Right/Click enters.
- ANOMALY: Left returns; Right/Click sets the main anomaly and exits.
- TRACKING: Up returns; Left/Right cycles track ID; Click confirms and exits.
- CONFIG: placeholder menu; Left returns. Subitems (invert axes, rotate display, RSSI calibration stub, version) reset selection to the first entry on entry and toggle with Right/Click.
- Anomaly HUD: four 30x30 XBM icons (see `lib/asap_display/src/asap/display/assets/AnomalyIcons.h`), arc radius 21 px with 3 px thickness starting at north, roman numerals baseline y=57.

## Persistence Roadmap
- Current toggles (invert X/Y, rotation, future RSSI calibration) are volatile. Plan for a shared `settings::load/save` API that runs before UI initialization on both targets.

## Quick Checklist Before You Commit
- `pio test -e native` passes and any snapshot changes are deliberate.
- Relevant embedded builds (`pio run -e detector` / beacon / artifact / anomaly) succeed when firmware paths change.
- Arduino path still meets MCU constraints.

