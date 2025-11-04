#pragma once

#include <stdint.h>
#include <asap/display/DisplayTypes.h>

#ifdef ARDUINO
#include <stdlib.h>
#include <Arduino.h>
#include <U8g2lib.h>

namespace asap::display {

class DetectorDisplay {
 public:
  explicit DetectorDisplay(const DisplayPins& pins);

  bool begin();
  void drawBootScreen(const char* versionText);
  void drawHeartbeatFrame(uint32_t uptimeMs);
  void showStatus(const char* line1, const char* line2);
  void showJoystick(::asap::input::JoyAction action);

  void renderCustom(const DisplayFrame& frame, FrameKind kind);

  void setRotation180(bool enabled);
  bool rotation180() const { return rotation180_; }

  void drawAnomalyIndicators(uint8_t radPercent, uint8_t thermPercent,
                             uint8_t chemPercent, uint8_t psyPercent,
                             uint8_t radStage, uint8_t thermStage,
                             uint8_t chemStage, uint8_t psyStage);

  const DisplayFrame& lastFrame() const { return lastFrame_; }
  FrameKind lastFrameKind() const { return lastKind_; }
  uint32_t beginCount() const { return beginCalls_; }

 private:
  void renderFrame(const DisplayFrame& frame);
  void drawSpinner(uint8_t activeIndex, uint16_t cx, uint16_t cy);
  void drawCentered(const char* text, uint16_t y);
  void drawMenuTag();
  void drawProgressBar(const DisplayFrame& frame);

  DisplayPins pins_;
  U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2_;
  bool initialized_;
  DisplayFrame lastFrame_;
  FrameKind lastKind_;
  uint32_t beginCalls_;
  bool rotation180_ = false;
};

} // namespace asap::display

#else

#include <asap/display/NativeDisplay.h>
namespace asap { namespace display {
using DetectorDisplay = ::asap::display::NativeDisplay;
} }

#endif
