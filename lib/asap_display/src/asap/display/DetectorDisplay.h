#pragma once

#include <stdint.h>

#ifdef ARDUINO
#include <stdlib.h>
#include <Arduino.h>
#include <U8g2lib.h>
#endif

namespace asap::display {

struct DisplayPins {
  uint32_t chipSelect;
  uint32_t dataCommand;
  uint32_t reset;
};

#ifdef ARDUINO

class DetectorDisplay {
 public:
  explicit DetectorDisplay(const DisplayPins& pins);

  bool begin();
  void drawBootScreen(const char* versionText);
  void drawHeartbeatFrame(uint32_t uptimeMs);
  void showStatus(const char* line1, const char* line2);

 private:
  DisplayPins pins_;
  U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2_;
  bool initialized_;
  uint8_t spinnerIndex_;

  void drawSpinner(uint16_t cx, uint16_t cy);
  void drawCentered(const char* text, uint16_t y);
};

#else

class DetectorDisplay {
 public:
  explicit DetectorDisplay(const DisplayPins&) {}
  bool begin() { return false; }
  void drawBootScreen(const char*) {}
  void drawHeartbeatFrame(uint32_t) {}
  void showStatus(const char*, const char*) {}
};

#endif  // ARDUINO

}  // namespace asap::display
