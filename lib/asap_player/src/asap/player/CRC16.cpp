#include "CRC16.h"

namespace asap::player
{

uint16_t computeCRC16(const uint8_t* data, size_t len)
{
  uint16_t crc = 0xFFFFu; // init
  for (size_t i = 0; i < len; ++i)
  {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit)
    {
      if (crc & 0x8000u)
      {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021u);
      }
      else
      {
        crc = static_cast<uint16_t>(crc << 1);
      }
    }
  }
  return crc;
}

} // namespace asap::player

