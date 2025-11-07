// CRC16.h
// CRC-16/CCITT-FALSE implementation and API.
#pragma once

#include <cstdint>
#include <cstddef>

namespace asap::player
{

// CRC-16/CCITT-FALSE parameters:
// - Polynomial: 0x1021
// - Init: 0xFFFF
// - ReflectIn/ReflectOut: false
// - XOROut: 0x0000
// Validate with test vector "123456789" -> 0x29B1.
uint16_t computeCRC16(const uint8_t* data, size_t len);

} // namespace asap::player
