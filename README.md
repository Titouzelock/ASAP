ASAP — Airsoft Stalker Anomalous Project
=======================================

Firmware for STM32F103C8T6 (Blue Pill) with a native host target for fast tests and UI snapshots. Built with PlatformIO and Unity unit tests, CI via GitHub Actions.

Quick Start
- Build device (Detector): `pio run -e detector`
- Upload to device: `pio run -e detector -t upload`
- Other roles: `pio run -e beacon|artifact|anomaly`
- Native tests + UI snapshots: `pio test -e native` (snapshots emitted to `snapshots/`)

Related Docs
- `AGENTS.md` — conventions, build/test matrix, MCU constraints
- `PROJECT_CONTEXT.md` — system overview and recent changes
- `UI.md` — UI navigation and contracts
- `display.md` — display geometry and rendering notes
- `native_test.md` — snapshot test flow

