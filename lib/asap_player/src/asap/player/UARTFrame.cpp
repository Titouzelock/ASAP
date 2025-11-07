#include "UARTFrame.h"
#include "CRC16.h"

namespace asap::player
{

bool encodeFrame(const uint8_t* payload,
                 uint16_t length,
                 uint8_t* outFrame,
                 uint16_t outCapacity,
                 uint16_t& outLen)
{
  if (!payload || !outFrame) { return false; }
  if (length == 0 || length > kMaxUartPayload) { return false; }

  const uint16_t needed = static_cast<uint16_t>(1 + 2 + length + 2 + 1);
  if (outCapacity < needed) { return false; }

  uint16_t pos = 0;
  outFrame[pos++] = kFrameSOH;
  outFrame[pos++] = static_cast<uint8_t>(length & 0xFFu);
  outFrame[pos++] = static_cast<uint8_t>((length >> 8) & 0xFFu);

  for (uint16_t i = 0; i < length; ++i)
  {
    outFrame[pos++] = payload[i];
  }

  const uint16_t crc = computeCRC16(payload, length);
  outFrame[pos++] = static_cast<uint8_t>(crc & 0xFFu);       // CRC_L
  outFrame[pos++] = static_cast<uint8_t>((crc >> 8) & 0xFFu); // CRC_H
  outFrame[pos++] = kFrameEOT;

  outLen = pos;
  return true;
}

bool decodeFrame(const uint8_t* inFrame,
                 uint16_t inLen,
                 uint8_t* outPayload,
                 uint16_t outCapacity,
                 uint16_t& outLen)
{
  outLen = 0;
  if (!inFrame || inLen < 1 + 2 + 2 + 1) { return false; }
  if (inFrame[0] != kFrameSOH) { return false; }

  uint16_t pos = 1;
  uint16_t length = static_cast<uint16_t>(inFrame[pos] | (static_cast<uint16_t>(inFrame[pos+1]) << 8));
  pos += 2;
  if (length == 0 || length > kMaxUartPayload) { return false; }

  const uint16_t needed = static_cast<uint16_t>(1 + 2 + length + 2 + 1);
  if (inLen < needed) { return false; }
  if (outCapacity < length) { return false; }

  for (uint16_t i = 0; i < length; ++i)
  {
    outPayload[i] = inFrame[pos++];
  }

  const uint16_t crcL = inFrame[pos++];
  const uint16_t crcH = inFrame[pos++];
  const uint16_t crc = static_cast<uint16_t>(crcL | (crcH << 8));
  if (inFrame[pos++] != kFrameEOT) { return false; }

  const uint16_t calc = computeCRC16(outPayload, length);
  if (calc != crc) { return false; }

  outLen = length;
  return true;
}

} // namespace asap::player

