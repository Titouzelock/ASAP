# PLAYER_DATA_ARCHITECTURE.md
*Author:* Internal design specification  
*Date:* 2025-11-04  
*Version:* 1.0  
*Purpose:* Defines the data model, storage layout, and interface methods for player state management on the embedded game device.  
*Scope:* Only applies to the player data subsystem. Does not define global firmware behavior or higher-level game logic.

---

## Overview
This module defines the data architecture for the **player state management system** used in the game device.  
It separates all information into two domains:

- **Permanent data** (stored in Flash, persistent across power cycles).  
- **Impermanent data** (stored in RAM, cleared on reboot or respawn).

It is designed for long-term scalability, forward compatibility, and simple import/export via UART.

---

## 1. Permanent Data

### 1.1 Description Block
- Fixed-size ASCII text field of **1000 bytes**.  
- Used for human-readable information (name, lore, equipment list, etc.).  
- Uses `\r` (0x0D) and `\n` (0x0A) for line breaks on a 3-line display.  
- Encoding: ASCII only.  
- Unused bytes are filled with 0x00.

### 1.2 Logical Modifiers
Each resistance and armor value ranges **0–10**.  
Money is stored as a 16-bit integer, each unit = 100 rubles.  
The faction field is an 8-bit value (0–255).  
An 8-byte reserved space is provided for future use.

```cpp
struct PlayerLogic {
    uint8_t  fire_resistance;       // 0–10
    uint8_t  psy_resistance;        // 0–10
    uint8_t  radiation_resistance;  // 0–10
    uint8_t  chemical_resistance;   // 0–10
    uint8_t  armor;                 // 0–10
    uint8_t  faction;               // 0–255
    uint16_t money_units;           // value * 100 = rubles
    uint8_t  reserved[8];           // expansion / alignment
};
```

### 1.3 System Configuration
Stores device-level settings such as brightness, volume, and language.  
Has its own reserved section for calibration or expansion.

```cpp
struct SystemConfig {
    uint8_t  brightness;     // 0–100%
    uint8_t  volume;         // 0–100%
    uint8_t  language;       // enum index
    uint8_t  display_mode;   // inverted or not
    uint16_t serial_number;  // device ID
    uint8_t  reserved[10];   // calibration, padding
};
```

### 1.4 Master Persistent Structure
A top-level container that holds all permanent fields, with versioning and CRC for integrity checking.

```cpp
struct PlayerPersistent {
    uint8_t       version;              // structure version
    char          description[1000];    // ASCII descriptive data
    PlayerLogic   logic;                // gameplay modifiers
    SystemConfig  system;               // device config
    uint16_t      crc;                  // CRC16 for data integrity
};
```

**CRC16** is computed over all fields except itself (standard Modbus or CCITT).

---

## 2. Impermanent (Session) Data

Represents session-only variables that reset on reboot or player respawn.  
Stored only in RAM.

```cpp
struct PlayerSession {
    uint8_t  version;               // structure version
    uint16_t fire_exposure;         // 0–1024 (1000 = lethal)
    uint16_t psy_exposure;          // 0–1024
    uint16_t radiation_exposure;    // 0–1024
    uint16_t chemical_exposure;     // 0–1024
    uint32_t life_timer_ms;         // milliseconds since respawn
};
```

Reset logic:
```cpp
void resetSession(PlayerSession& s);
```

---

## 3. Global Container

A unified structure combining both domains for convenient access.

```cpp
struct PlayerState {
    PlayerPersistent persistent;
    PlayerSession    session;
};
```

---

## 4. Core Methods (to implement)

All functions are defined as stubs; implementation is expected using HAL Flash APIs and standard UART I/O.

```cpp
// Load player data from flash into RAM
bool loadPersistent(PlayerPersistent& dst);

// Save player data from RAM to flash
bool savePersistent(const PlayerPersistent& src);

// Export player data via serial (binary or ASCII-encoded)
void exportPersistent(const PlayerPersistent& src);

// Import player data via serial
bool importPersistent(PlayerPersistent& dst);

// Reset impermanent data
void resetSession(PlayerSession& s);

// Compute CRC16 of a data block
uint16_t computeCRC16(const uint8_t* data, size_t len);
```

---

## 5. Versioning and Compatibility
- Each struct begins with a `version` field.  
- On load, check if the version matches the firmware’s expected version.  
- If not, apply a migration routine or reset to defaults.  
- Always validate the `crc` field before accepting persistent data.

---

## 6. Storage Model
- Persistent data is stored in Flash memory using a reserved page.  
- The system maintains one active copy; backups or redundancy can be added later.  
- Impermanent data is held in RAM only.  
- All saves and loads must be atomic (write only after successful CRC verification).

---

## 7. Serial Communication Notes
UART-based interface, default 115200 bps, 8N1.  
Used for import/export and debugging.  
Packet framing and command structure will be defined later.  
Stubs are provided for compatibility with future protocol expansion.

---

## 8. Summary
This document defines a compact, scalable player data system capable of:
- Storing descriptive and logical player data without reflashing.  
- Supporting future firmware versions through versioned structs.  
- Exchanging player profiles via UART.  
- Maintaining a clear separation between permanent and impermanent states.
