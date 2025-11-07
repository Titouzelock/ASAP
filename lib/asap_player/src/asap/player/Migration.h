#pragma once

#include "PlayerData.h"

namespace asap::player
{

// Attempt to migrate from p_in.version to current kPersistentVersion.
// Returns true if migration applied; false if no handler or failure.
// Callers should fall back to initDefaults() when false.
bool migratePersistent(const PlayerPersistent& p_in, PlayerPersistent& p_out);

} // namespace asap::player

