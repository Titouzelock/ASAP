#pragma once

#include <cstdint>
#include <cstddef>

namespace asap::player
{

// Versions
constexpr uint8_t kPersistentVersion = 1;
constexpr uint8_t kSessionVersion    = 1;

// Ranges and limits
constexpr uint8_t  kMinResistance = 0;
constexpr uint8_t  kMaxResistance = 10;
constexpr uint8_t  kMinPercent    = 0;
constexpr uint8_t  kMaxPercent    = 100;
constexpr uint16_t kMinExposure   = 0;
constexpr uint16_t kMaxExposure   = 1024;   // 1000 lethal, 1024 headroom
constexpr uint16_t kLethalExposure = 1000;

// Defaults (persistent)
constexpr uint8_t  kDefaultFireRes       = 0;
constexpr uint8_t  kDefaultPsyRes        = 0;
constexpr uint8_t  kDefaultRadRes        = 0;
constexpr uint8_t  kDefaultChemRes       = 0;
constexpr uint8_t  kDefaultArmor         = 0;
constexpr uint8_t  kDefaultFaction       = 0;      // 0 = neutral
constexpr uint16_t kDefaultMoneyUnits    = 0;      // units *100 = rubles
constexpr uint8_t  kDefaultBrightness    = 80;     // %
constexpr uint8_t  kDefaultVolume        = 70;     // %
constexpr uint8_t  kDefaultLanguage      = 0;      // 0 = English (enum placeholder)
constexpr uint8_t  kDefaultDisplayMode   = 0;      // 0 normal, 1 rotated 180°
constexpr uint16_t kDefaultSerialNumber  = 0xFFFF; // unassigned

// Defaults (session)
constexpr uint16_t kDefaultExposure      = 0;
constexpr uint32_t kDefaultLifeTimerMs   = 0;

// Description field
constexpr size_t   kDescriptionSize      = 960;
extern const char  kDefaultDescription[]; // "UNREGISTERED PLAYER\r\nAwaiting configuration..."

// Clamp helper
template <typename T>
inline T clamp(T v, T lo, T hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

#pragma pack(push, 1)

struct PlayerLogic
{
  uint8_t  fire_resistance;       // 0..10
  uint8_t  psy_resistance;        // 0..10
  uint8_t  radiation_resistance;  // 0..10
  uint8_t  chemical_resistance;   // 0..10
  uint8_t  armor;                 // 0..10
  uint8_t  faction;               // 0..255
  uint16_t money_units;           // value * 100 = rubles
  uint8_t  reserved[8];           // expansion / alignment
}; // 16 bytes

struct SystemConfig
{
  uint8_t  brightness;     // 0..100%
  uint8_t  volume;         // 0..100%
  uint8_t  language;       // enum index (0 = English)
  uint8_t  display_mode;   // 0 normal, 1 rotated 180°
  uint16_t serial_number;  // device ID
  uint8_t  reserved[10];   // calibration, padding
}; // 16 bytes

struct PlayerPersistent
{
  uint8_t       version;                    // structure version
  char          description[kDescriptionSize]; // ASCII descriptive data
  PlayerLogic   logic;                      // gameplay modifiers
  SystemConfig  system;                     // device config
  uint16_t      crc;                        // CRC16 (CCITT-FALSE) little-endian
}; // expected 995 bytes

struct PlayerSession
{
  uint8_t  version;             // structure version
  uint16_t fire_exposure;       // 0..1024 (1000 lethal)
  uint16_t psy_exposure;        // 0..1024
  uint16_t radiation_exposure;  // 0..1024
  uint16_t chemical_exposure;   // 0..1024
  uint32_t life_timer_ms;       // milliseconds since respawn
}; // expected 13 bytes

struct PlayerState
{
  PlayerPersistent persistent;
  PlayerSession    session;
};

#pragma pack(pop)

// Size guards (single 1KB page for persistent)
static_assert(sizeof(PlayerLogic) == 16, "PlayerLogic must be 16 bytes");
static_assert(sizeof(SystemConfig) == 16, "SystemConfig must be 16 bytes");
static_assert(sizeof(PlayerPersistent) <= 1024, "PlayerPersistent must fit one page");
static_assert(sizeof(PlayerPersistent) == 995, "PlayerPersistent must be 995 bytes");
static_assert(sizeof(PlayerSession) == 13, "PlayerSession must be 13 bytes");

// Initialization and clamping
void initDefaults(PlayerPersistent &p);
void initDefaults(PlayerSession &s);
void clampPersistent(PlayerPersistent &p);
void clampSession(PlayerSession &s);

} // namespace asap::player

