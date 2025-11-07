#pragma once

#include <cstdint>

namespace asap::player
{

// STM32F103C8T6 flash geometry
constexpr uint32_t kFlashPageSize = 1024;

// Persistent storage location (last 1KB page)
constexpr uint32_t kFlashBaseAddr_PlayerPersistent = 0x0800FC00u;

// Optional future backup/staging page (commented for now)
// constexpr uint32_t kFlashBackupAddr_PlayerPersistent = 0x0800F800u;

// UART framing policy
constexpr uint16_t kMaxUartPayload = 1024; // one page

#ifndef ARDUINO
// Persist to a file as a byte-accurate stand-in for flash
constexpr const char* kNativeStubPath = "test/.tmp/player_persistent.bin";
#endif

} // namespace asap::player

