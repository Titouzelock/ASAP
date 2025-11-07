// Storage_native.cpp
// Native (host) implementation of persistence via a file stub.
// The file acts as a byte-accurate stand-in for flash memory.
#ifndef ARDUINO

#include "Storage.h"
#include "StorageConfig.h"
#include "CRC16.h"
#include "Migration.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace asap::player
{

// Read entire persistent blob from the stub file.
static bool readAll(void* dst, size_t len)
{
  std::FILE* f = std::fopen(kNativeStubPath, "rb");
  if (!f)
  {
    return false;
  }
  const size_t n = std::fread(dst, 1, len, f);
  std::fclose(f);
  return n == len;
}

// Write entire persistent blob to the stub file (creating directory if needed).
static bool writeAll(const void* src, size_t len)
{
  std::filesystem::create_directories(std::filesystem::path(kNativeStubPath).parent_path());
  std::FILE* f = std::fopen(kNativeStubPath, "wb");
  if (!f)
  {
    return false;
  }
  const size_t n = std::fwrite(src, 1, len, f);
  std::fclose(f);
  return n == len;
}

// Load and validate persistent data from the stub file.
bool loadPersistent(PlayerPersistent& dst)
{
  PlayerPersistent tmp{};
  if (!readAll(&tmp, sizeof(tmp)))
  {
    // Initialize defaults if file missing
    initDefaults(dst);
    return false;
  }

  // Verify CRC
  const uint16_t storedCrc = tmp.crc;
  tmp.crc = 0;
  const uint16_t calc = computeCRC16(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
  if (calc != storedCrc)
  {
    initDefaults(dst);
    return false;
  }

  // Version check
  if (tmp.version != kPersistentVersion)
  {
    PlayerPersistent migrated{};
    if (!migratePersistent(tmp, migrated))
    {
      initDefaults(dst);
      return false;
    }
    dst = migrated;
  }
  else
  {
    dst = tmp;
  }

  // Clamp fields after load
  clampPersistent(dst);
  return true;
}

// Save persistent data with CRC computed over the packed struct.
bool savePersistent(const PlayerPersistent& src)
{
  PlayerPersistent tmp = src;
  // Ensure version and clamping
  tmp.version = kPersistentVersion;
  clampPersistent(tmp);
  // Compute CRC over structure with crc=0
  tmp.crc = 0u;
  const uint16_t crc = computeCRC16(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
  tmp.crc = crc; // little-endian store
  return writeAll(&tmp, sizeof(tmp));
}

// For native stub, keep import/export unimplemented against a serial port.
// Tests should exercise UARTFrame encode/decode directly.
// Serial import is not implemented in native stub; tests exercise framing directly.
bool importPersistent(PlayerPersistent& dst)
{
  (void)dst;
  return false;
}

// Serial export is not implemented in native stub.
void exportPersistent(const PlayerPersistent& src)
{
  (void)src;
}

void resetSession(PlayerSession& s)
{
  initDefaults(s);
}

bool load(PlayerState& state)
{
  bool ok = loadPersistent(state.persistent);
  initDefaults(state.session);
  return ok;
}

bool save(const PlayerState& state)
{
  return savePersistent(state.persistent);
}

void resetSession(PlayerState& state)
{
  resetSession(state.session);
}

} // namespace asap::player

#endif // !ARDUINO
