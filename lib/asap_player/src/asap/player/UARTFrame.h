#pragma once

#include <cstdint>
#include <cstddef>
#include "StorageConfig.h"

namespace asap::player
{

// [0x02][len_L][len_H][payload][CRC_L][CRC_H][0x03]
constexpr uint8_t kFrameSOH = 0x02;
constexpr uint8_t kFrameEOT = 0x03;

// Encode payload into outFrame. Returns true on success.
// - length must be in [1, kMaxUartPayload]
// - outCapacity must be >= 1 + 2 + length + 2 + 1
bool encodeFrame(const uint8_t* payload,
                 uint16_t length,
                 uint8_t* outFrame,
                 uint16_t outCapacity,
                 uint16_t& outLen);

// Decode inFrame into outPayload. Returns true on success with CRC verified.
// - outCapacity must be >= decoded payload length
// - Reject zero-length or >kMaxUartPayload
bool decodeFrame(const uint8_t* inFrame,
                 uint16_t inLen,
                 uint8_t* outPayload,
                 uint16_t outCapacity,
                 uint16_t& outLen);

} // namespace asap::player

