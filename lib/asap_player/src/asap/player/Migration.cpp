// Migration.cpp
// Default no-op migration. Add versioned handlers here when layouts change.
#include "Migration.h"

namespace asap::player
{

bool migratePersistent(const PlayerPersistent& p_in, PlayerPersistent& p_out)
{
  // No migrations defined yet; return false to signal caller to use defaults
  (void)p_in;
  (void)p_out;
  return false;
}

} // namespace asap::player
