// Storage_embedded.cpp
// Embedded implementation of persistence using HAL Flash APIs.
// The detector keeps one packed PlayerPersistent struct in the last flash
// page (see StorageConfig). On save we erase/program that page and verify
// by reading it back and recomputing the CRC.
//
// This file is only compiled for embedded targets (ASAP_EMBEDDED). The
// native (host) backend lives in Storage_native.cpp.

#ifdef ASAP_EMBEDDED

#include "Storage.h"
#include "StorageConfig.h"
#include "CRC16.h"
#include "Migration.h"

#if defined(STM32F1xx) || defined(STM32F1)
#include "stm32f1xx_hal.h"
#endif

namespace asap::player
{

// Byte-wise flash read into RAM buffer. This treats flash like a const
// array of bytes and copies into a RAM struct.
static void readFlash(uint32_t addr, void* dst, size_t len)
{
  const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
  uint8_t* d = reinterpret_cast<uint8_t*>(dst);
  for (size_t i = 0; i < len; ++i)
  {
    d[i] = p[i];
  }
}

#if defined(STM32F1xx) || defined(STM32F1)
// Erase a single flash page at the given address.
static bool erasePage(uint32_t addr)
{
  HAL_FLASH_Unlock();
  FLASH_EraseInitTypeDef erase{};
  erase.TypeErase   = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = addr;
  erase.NbPages     = 1;
  uint32_t pageError = 0;
  HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &pageError);
  HAL_FLASH_Lock();
  return st == HAL_OK;
}

// Program packed bytes into flash using halfword programming.
// Tail byte is handled by patching the existing halfword.
static bool programBytes(uint32_t addr, const uint8_t* src, size_t len)
{
  HAL_FLASH_Unlock();
  // Program halfwords (16-bit) where possible
  size_t i = 0;
  while (i + 1 < len)
  {
    uint16_t hw = static_cast<uint16_t>(src[i] | (static_cast<uint16_t>(src[i+1]) << 8));
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, hw) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return false;
    }
    i += 2;
  }
  // Tail byte if odd length
  if (i < len)
  {
    // Read existing halfword then patch low byte
    uint16_t hw = *reinterpret_cast<volatile const uint16_t*>(addr + i);
    hw = static_cast<uint16_t>((hw & 0xFF00u) | src[i]);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, hw) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return false;
    }
  }
  HAL_FLASH_Lock();
  return true;
}
#endif

// Load and validate persistent data from flash.
bool loadPersistent(PlayerPersistent& dst)
{
  PlayerPersistent tmp{};
  readFlash(kFlashBaseAddr_PlayerPersistent, &tmp, sizeof(tmp));

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

  clampPersistent(dst);
  return true;
}

// Compute CRC, erase page, program bytes, and read-back verify.
bool savePersistent(const PlayerPersistent& src)
{
  PlayerPersistent tmp = src;
  tmp.version = kPersistentVersion;
  clampPersistent(tmp);
  tmp.crc = 0u;
  const uint16_t crc = computeCRC16(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
  tmp.crc = crc;

#if defined(STM32F1xx) || defined(STM32F1)
  if (!erasePage(kFlashBaseAddr_PlayerPersistent))
  {
    return false;
  }
  if (!programBytes(kFlashBaseAddr_PlayerPersistent, reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp)))
  {
    return false;
  }
  // Read-back verify
  PlayerPersistent verify{};
  readFlash(kFlashBaseAddr_PlayerPersistent, &verify, sizeof(verify));
  uint16_t savedCrc = verify.crc;
  verify.crc = 0;
  uint16_t calc2 = computeCRC16(reinterpret_cast<const uint8_t*>(&verify), sizeof(verify));
  return calc2 == savedCrc;
#else
  // If HAL not available, signal failure to avoid undefined behavior.
  (void)tmp;
  return false;
#endif
}

// NOTE: import/export via UART to be implemented within the project serial layer if needed.
// UART import/export to be implemented against the project's serial layer.
bool importPersistent(PlayerPersistent& dst)
{
  (void)dst;
  return false;
}

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

#endif  // ASAP_EMBEDDED
