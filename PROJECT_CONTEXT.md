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

### Detector Display Notes

- Wiring (SPI 4-wire): `PA5` SCK, `PA7` MOSI, `PA4` CS, `PA3` DC, `PA2` RST, +3V3/GND for power.
- Library scaffold: `lib/asap_display` exposes `DetectorDisplay` (boot splash + heartbeat spinner).
- Heartbeat demo: refreshes every 250 ms with uptime text; spinner centers on the 256x64 canvas.
- Native mock: `env:native` reuses the driver to capture frames, validated via Unity tests.
- Snapshot export: native driver rasterises text/spinner frames into grayscale PGM snapshots for CI review.
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

