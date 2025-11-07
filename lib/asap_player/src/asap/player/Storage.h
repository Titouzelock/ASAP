// Storage.h
// Persistence API for PlayerPersistent on embedded and native builds.
#pragma once

#include <cstdint>
#include "PlayerData.h"

namespace asap::player
{

// Persistent storage
// Embedded: uses STM32 HAL Flash APIs at kFlashBaseAddr_PlayerPersistent (erase/program, read-back verify).
// Native: file-backed stub at test/.tmp/player_persistent.bin (same layout/CRC/versioning).
bool loadPersistent(PlayerPersistent& dst);
bool savePersistent(const PlayerPersistent& src);

// Import/export via UART framing (binary). Payload is raw packed struct bytes.
// - importPersistent applies version/migration, clamps, computes CRC, and saves on success.
bool importPersistent(PlayerPersistent& dst);
void exportPersistent(const PlayerPersistent& src);

// Session helpers
void resetSession(PlayerSession& s);

// Convenience façade operating on combined state
bool load(PlayerState& state);
bool save(const PlayerState& state);
void resetSession(PlayerState& state);

} // namespace asap::player
