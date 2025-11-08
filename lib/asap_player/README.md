Player Data Module Overview

This module provides compact, packed data structures and persistence utilities for player data on STM32F103 (embedded) and a native host (tests/tools). It is designed to fit in a single 1 KB flash page, use CRC-16/CCITT-FALSE for integrity, and support future migrations.

Contents
- Data model (packed structs): `PlayerLogic`, `SystemConfig`, `PlayerSession`, `PlayerPersistent`, `PlayerState`
- Persistence API: `loadPersistent`, `savePersistent`, `load`, `save`, `resetSession`
- CRC utilities: `computeCRC16`
- UART framing helpers: `encodeFrame`, `decodeFrame`
- Migration hook: `migratePersistent`
- Build config: flash addresses and limits in `StorageConfig.h`

Key Files
- `lib/asap_player/src/asap/player/PlayerData.h(.cpp)` - data structures, defaults, clamping
- `lib/asap_player/src/asap/player/Storage.h` - persistence API (embedded + native)
- `lib/asap_player/src/asap/player/Storage_embedded.cpp` - STM32 HAL Flash implementation
- `lib/asap_player/src/asap/player/Storage_native.cpp` - native file-backed implementation
- `lib/asap_player/src/asap/player/CRC16.h(.cpp)` - CRC-16/CCITT-FALSE
- `lib/asap_player/src/asap/player/UARTFrame.h(.cpp)` - binary frame encode/decode
- `lib/asap_player/src/asap/player/StorageConfig.h` - flash base address and UART payload limit
- `lib/asap_player/src/asap/player/Migration.h(.cpp)` - migration hook (no-op by default)

Data Model
- `PlayerPersistent` (flash):
  - Layout: `version`, `description[960]`, `PlayerLogic`, `SystemConfig`, `crc`
  - Size: 995 bytes (fits one 1 KB page)
  - CRC: CRC-16/CCITT-FALSE over the struct bytes with `crc = 0`
  - Versioning: only top-level struct has `version` (substructures inherit semantics)
- `PlayerSession` (RAM):
  - Session metrics (exposures, timer), versioned, reset on reboot/respawn
- `PlayerState` combines both for convenience

Packing & Endianness
- `#pragma pack(push, 1)` is used; the serialized representation equals in-memory bytes
- All multi-byte fields are little-endian (native MCU order)

Flash Addressing (STM32F103C8T6)
- Reserved page for persistent blob: `0x0800FC00` (last 1 KB of flash)
- Config constants in `StorageConfig.h`:
  - `kFlashBaseAddr_PlayerPersistent = 0x0800FC00`
  - `kFlashPageSize = 1024`
  - `kMaxUartPayload = 1024`

CRC Details
- Variant: CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`, no reflection, XOR-out `0x0000`)
- Test vector: "123456789" -> `0x29B1`

UART Framing
- Binary format: `[0x02][len_L][len_H][payload][CRC_L][CRC_H][0x03]`
- Payload: raw packed struct bytes (`PlayerPersistent` or `PlayerSession`)
- Length bounds: 1..1024 (reject zero or >1024)
- CRC computed over `payload` only; stored little-endian

Persistence API (How It Connects)
- Embedded path (`Storage_embedded.cpp`):
  - `loadPersistent(dst)` reads flash, verifies CRC and version
    - If version mismatch: `migratePersistent(old, migrated)` -> on failure, `initDefaults(dst)`
    - Always `clampPersistent(dst)` after load/migration
  - `savePersistent(src)` clamps, computes CRC, erases page, programs halfwords, read-back verifies
  - `importPersistent(dst)` / `exportPersistent(src)` are hooks for UART workflows (to be wired to your serial layer)
- Native path (`Storage_native.cpp`):
  - Same semantics as embedded, but persists to `test/.tmp/player_persistent.bin`
  - Useful for unit tests and tooling without hardware

Typical Flows
- First boot / defaults:
  - `PlayerState s; initDefaults(s.persistent); initDefaults(s.session); save(s.persistent);`
- Load at startup:
  - `PlayerState s; load(s);`  // persistent from flash/file, session defaults
- Save after changes:
  - `save(state);`  // writes persistent only
- Reset session on respawn:
  - `resetSession(state);`

Migration
- `bool migratePersistent(const PlayerPersistent& in, PlayerPersistent& out)`
  - Return `true` after populating `out` with the current layout/version
  - Return `false` to fall back to defaults (`initDefaults`) when no handler exists
  - Add versioned handlers as the schema evolves

Testing
- Native unit tests live under `test/test_player_data/`
  - CRC test (0x29B1), size guards, defaults/clamping, UART framing round-trip, native save/load round-trip
- Snapshot/UI tests are driven by `test/test_main.cpp` (native only)

Framing + CRC Round-Trip Test
- Implemented in `test/test_player_data/test_player_data.cpp` as `test_uart_frame_playerpersistent_roundtrip`.
- Wired into the native runner `test/test_main.cpp` and executed with the snapshot tests.
- Validates end-to-end: pack PlayerPersistent -> encodeFrame -> decodeFrame -> CRC recheck -> byte-for-byte equality.
- Includes a negative path: flips a payload byte in the frame and asserts `decodeFrame` fails due to CRC mismatch.

Notes
- Keep ARDUINO and native parity: same struct layout, CRC, and versioning behaviors
- Avoid dynamic allocation in embedded paths; use fixed buffers
- Do not modify third-party code under `lib/Unity/`

