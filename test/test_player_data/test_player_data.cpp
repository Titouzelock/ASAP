#include <unity.h>
#include <cstdio>
#include <cstring>

#include <asap/player/CRC16.h>
#include <asap/player/PlayerData.h>
#include <asap/player/Storage.h>
#include <asap/player/UARTFrame.h>

using namespace asap::player;

void test_crc16_vector(void)
{
  const char* s = "123456789";
  const uint16_t crc = computeCRC16(reinterpret_cast<const uint8_t*>(s), 9);
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc);
}

void test_sizes(void)
{
  TEST_ASSERT_EQUAL_UINT32(16, sizeof(PlayerLogic));
  TEST_ASSERT_EQUAL_UINT32(16, sizeof(SystemConfig));
  TEST_ASSERT_EQUAL_UINT32(995, sizeof(PlayerPersistent));
  TEST_ASSERT_TRUE(sizeof(PlayerPersistent) <= 1024);
  TEST_ASSERT_EQUAL_UINT32(13, sizeof(PlayerSession));
}

void test_defaults_and_clamp(void)
{
  PlayerPersistent p{};
  initDefaults(p);
  TEST_ASSERT_EQUAL_UINT8(kPersistentVersion, p.version);
  TEST_ASSERT_EQUAL_UINT8(kDefaultBrightness, p.system.brightness);
  TEST_ASSERT_EQUAL_UINT8(kDefaultVolume, p.system.volume);
  TEST_ASSERT_EQUAL_UINT8(0, p.system.display_mode);

  p.logic.fire_resistance = 250; // invalid high
  clampPersistent(p);
  TEST_ASSERT_EQUAL_UINT8(kMaxResistance, p.logic.fire_resistance);

  PlayerSession s{};
  initDefaults(s);
  TEST_ASSERT_EQUAL_UINT8(kSessionVersion, s.version);
  s.fire_exposure = 70000; // overflow -> truncated in assignment
  clampSession(s);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(kMaxExposure, s.fire_exposure);
}

void test_uart_frame_roundtrip(void)
{
  uint8_t payload[32];
  for (uint16_t i = 0; i < sizeof(payload); ++i) payload[i] = static_cast<uint8_t>(i);

  uint8_t frame[1 + 2 + 32 + 2 + 1];
  uint16_t outLen = 0;
  TEST_ASSERT_TRUE(encodeFrame(payload, 32, frame, sizeof(frame), outLen));

  uint8_t decoded[64];
  uint16_t decLen = 0;
  TEST_ASSERT_TRUE(decodeFrame(frame, outLen, decoded, sizeof(decoded), decLen));
  TEST_ASSERT_EQUAL_UINT16(32, decLen);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, decoded, 32);

  // Reject zero length
  TEST_ASSERT_FALSE(encodeFrame(payload, 0, frame, sizeof(frame), outLen));
}

void test_persistent_file_roundtrip(void)
{
  PlayerPersistent p{};
  initDefaults(p);
  // Mutate some fields
  p.system.brightness = 42;
  p.logic.armor = 7;
  TEST_ASSERT_TRUE(savePersistent(p));

  PlayerPersistent q{};
  TEST_ASSERT_TRUE(loadPersistent(q));
  TEST_ASSERT_EQUAL_UINT8(42, q.system.brightness);
  TEST_ASSERT_EQUAL_UINT8(7, q.logic.armor);
}
