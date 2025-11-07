#pragma once

#include <cstdint>
#include <cstddef>

namespace asap::player
{

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no XOR-out.
// Validate with "123456789" -> 0x29B1.
uint16_t computeCRC16(const uint8_t* data, size_t len);

} // namespace asap::player

