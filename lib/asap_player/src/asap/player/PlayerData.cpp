// PlayerData.cpp
// Default initialization and clamping helpers for player structures.
#include "PlayerData.h"

namespace asap::player
{

// Default ASCII description persisted in flash when uninitialized.
const char kDefaultDescription[] = "UNREGISTERED PLAYER\r\nAwaiting configuration...";

// Populate persistent structure with safe defaults. CRC is left to be computed by savePersistent.
void initDefaults(PlayerPersistent &p)
{
  p.version = kPersistentVersion;
  // description
  // Fill with zeros then copy default string
  for (size_t i = 0; i < kDescriptionSize; ++i)
  {
    p.description[i] = '\0';
  }
  const char *src = kDefaultDescription;
  size_t idx = 0;
  while (src[idx] != '\0' && idx < kDescriptionSize)
  {
    p.description[idx] = src[idx];
    ++idx;
  }

  // logic defaults
  p.logic.fire_resistance      = kDefaultFireRes;
  p.logic.psy_resistance       = kDefaultPsyRes;
  p.logic.radiation_resistance = kDefaultRadRes;
  p.logic.chemical_resistance  = kDefaultChemRes;
  p.logic.armor                = kDefaultArmor;
  p.logic.faction              = kDefaultFaction;
  p.logic.money_units          = kDefaultMoneyUnits;
  for (int i = 0; i < 8; ++i) { p.logic.reserved[i] = 0; }

  // system defaults
  p.system.brightness    = kDefaultBrightness;
  p.system.volume        = kDefaultVolume;
  p.system.language      = kDefaultLanguage; // 0 = English
  p.system.display_mode  = kDefaultDisplayMode; // 0 normal
  p.system.serial_number = kDefaultSerialNumber;
  for (int i = 0; i < 10; ++i) { p.system.reserved[i] = 0; }

  // CRC is computed during save; set to 0 for now.
  p.crc = 0u;
}

// Populate session structure with safe defaults (RAM-only data).
void initDefaults(PlayerSession &s)
{
  s.version            = kSessionVersion;
  s.fire_exposure      = kDefaultExposure;
  s.psy_exposure       = kDefaultExposure;
  s.radiation_exposure = kDefaultExposure;
  s.chemical_exposure  = kDefaultExposure;
  s.life_timer_ms      = kDefaultLifeTimerMs;
}

// Enforce ranges and normalize boolean flags for persistent data after load/import.
void clampPersistent(PlayerPersistent &p)
{
  p.logic.fire_resistance      = clamp<uint8_t>(p.logic.fire_resistance,      kMinResistance, kMaxResistance);
  p.logic.psy_resistance       = clamp<uint8_t>(p.logic.psy_resistance,       kMinResistance, kMaxResistance);
  p.logic.radiation_resistance = clamp<uint8_t>(p.logic.radiation_resistance, kMinResistance, kMaxResistance);
  p.logic.chemical_resistance  = clamp<uint8_t>(p.logic.chemical_resistance,  kMinResistance, kMaxResistance);
  p.logic.armor                = clamp<uint8_t>(p.logic.armor,                kMinResistance, kMaxResistance);
  // faction unrestricted (0..255)
  // money_units unrestricted (uint16_t)

  p.system.brightness   = clamp<uint8_t>(p.system.brightness, kMinPercent, kMaxPercent);
  p.system.volume       = clamp<uint8_t>(p.system.volume,     kMinPercent, kMaxPercent);
  // language left as-is (enum index)
  p.system.display_mode = (p.system.display_mode == 0u || p.system.display_mode == 1u) ? p.system.display_mode : 0u;
}

// Enforce ranges for session data.
void clampSession(PlayerSession &s)
{
  s.fire_exposure      = clamp<uint16_t>(s.fire_exposure,      kMinExposure, kMaxExposure);
  s.psy_exposure       = clamp<uint16_t>(s.psy_exposure,       kMinExposure, kMaxExposure);
  s.radiation_exposure = clamp<uint16_t>(s.radiation_exposure, kMinExposure, kMaxExposure);
  s.chemical_exposure  = clamp<uint16_t>(s.chemical_exposure,  kMinExposure, kMaxExposure);
  // life_timer_ms unrestricted (monotonic counter)
}

} // namespace asap::player
