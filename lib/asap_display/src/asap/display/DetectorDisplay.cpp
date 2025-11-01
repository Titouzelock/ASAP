#include "asap/display/DetectorDisplay.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>
#include <stddef.h>

namespace asap::display {

namespace {
constexpr uint16_t kDisplayWidth = 256;
constexpr uint16_t kDisplayHeight = 64;

void formatFirmwareLine(char* dest, size_t destSize, const char* versionText) {
  if (!dest || destSize == 0) {
    return;
  }

  const char prefix[] = "FW ";
  size_t index = 0;

  while (index + 1 < destSize && prefix[index] != '\0') {
    dest[index] = prefix[index];
    ++index;
  }

  if (versionText) {
    size_t versionIndex = 0;
    while (versionText[versionIndex] != '\0' && index + 1 < destSize) {
      dest[index++] = versionText[versionIndex++];
    }
  }

  dest[index] = '\0';
}

void formatUptimeLine(char* dest, size_t destSize, uint32_t seconds) {
  if (!dest || destSize < 3) {
    return;
  }

  const char prefix[] = "Uptime ";
  size_t index = 0;
  while (prefix[index] != '\0' && index + 1 < destSize) {
    dest[index] = prefix[index];
    ++index;
  }

  char digits[11];
  size_t digitCount = 0;
  do {
    digits[digitCount++] = static_cast<char>('0' + (seconds % 10U));
    seconds /= 10U;
  } while (seconds > 0 && digitCount < sizeof(digits));

  while (digitCount > 0 && index + 1 < destSize) {
    dest[index++] = digits[--digitCount];
  }

  if (index + 2 <= destSize) {
    dest[index++] = 's';
  }

  dest[index < destSize ? index : destSize - 1] = '\0';
}
}  // namespace

DetectorDisplay::DetectorDisplay(const DisplayPins& pins)
    : pins_(pins),
      u8g2_(U8G2_R0,
            static_cast<uint8_t>(pins_.chipSelect),
            static_cast<uint8_t>(pins_.dataCommand),
            static_cast<uint8_t>(pins_.reset)),
      initialized_(false),
      spinnerIndex_(0) {}

bool DetectorDisplay::begin() {
  if (initialized_) {
    return true;
  }

  u8g2_.begin();
  u8g2_.setContrast(255);
  u8g2_.setFontMode(1);
  u8g2_.setDrawColor(1);
  u8g2_.setFontDirection(0);
  u8g2_.clearBuffer();

  initialized_ = true;
  return true;
}

void DetectorDisplay::drawBootScreen(const char* versionText) {
  if (!initialized_ && !begin()) {
    return;
  }

  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_ncenB14_tr);
  drawCentered("ASAP DETECTOR", 26);

  u8g2_.setFont(u8g2_font_6x13_tr);
  drawCentered("Initializing display...", 44);

  if (versionText && versionText[0] != '\0') {
    char buffer[32];
    formatFirmwareLine(buffer, sizeof(buffer), versionText);
    drawCentered(buffer, 60);
  }

  u8g2_.sendBuffer();
}

void DetectorDisplay::drawHeartbeatFrame(uint32_t uptimeMs) {
  if (!initialized_) {
    return;
  }

  u8g2_.clearBuffer();

  u8g2_.setFont(u8g2_font_6x13_tr);
  drawCentered("Detector ready", 20);

  char uptimeBuffer[24];
  const uint32_t seconds = uptimeMs / 1000U;
  formatUptimeLine(uptimeBuffer, sizeof(uptimeBuffer), seconds);
  drawCentered(uptimeBuffer, 52);

  spinnerIndex_ = static_cast<uint8_t>((uptimeMs / 150U) % 4U);
  drawSpinner(kDisplayWidth / 2U, kDisplayHeight / 2U);

  u8g2_.sendBuffer();
}

void DetectorDisplay::showStatus(const char* line1, const char* line2) {
  if (!initialized_) {
    return;
  }

  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_6x13_tr);

  if (line1 && line1[0] != '\0') {
    drawCentered(line1, 28);
  }
  if (line2 && line2[0] != '\0') {
    drawCentered(line2, 48);
  }

  u8g2_.sendBuffer();
}

void DetectorDisplay::drawSpinner(uint16_t cx, uint16_t cy) {
  static const int8_t offsets[4][2] = {
      {0, -12},
      {12, 0},
      {0, 12},
      {-12, 0},
  };

  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t x = static_cast<int16_t>(cx) + offsets[i][0];
    const int16_t y = static_cast<int16_t>(cy) + offsets[i][1];
    if (i == spinnerIndex_) {
      u8g2_.drawDisc(x, y, 6);
    } else {
      u8g2_.drawCircle(x, y, 4);
    }
  }
}

void DetectorDisplay::drawCentered(const char* text, uint16_t y) {
  if (!text) {
    return;
  }

  const int16_t width = u8g2_.getStrWidth(text);
  const int16_t x =
      static_cast<int16_t>((kDisplayWidth - static_cast<uint16_t>(width)) / 2);
  u8g2_.drawStr(x, y, text);
}

}  // namespace asap::display

#endif  // ARDUINO
