#pragma once

#ifndef ARDUINO

#include <stdint.h>
#include <vector>

#include "asap/display/DisplayTypes.h"

extern "C"
{
#include "u8g2.h"
}

namespace asap::display
{

class NativeDisplay
{
 public:
  explicit NativeDisplay(const DisplayPins& pins);

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

  FrameKind lastFrameKind() const { return lastKind_; }
  const DisplayFrame& lastFrame() const { return lastFrame_; }
  uint32_t beginCount() const { return beginCalls_; }

  const std::vector<uint8_t>& pixelBuffer() const { return pixel4bit_; }
  bool writeSnapshot(const char* filePath) const;

 private:
  void renderFrame(const DisplayFrame& frame, FrameKind kind);
  void drawSpinner(uint8_t activeIndex, uint16_t cx, uint16_t cy);
  void drawCentered(const char* text, uint16_t y);
  void drawMenuTag();
  void drawProgressBar(const DisplayFrame& frame);
  void captureBuffer();

  DisplayPins pins_;
  bool initialized_;
  FrameKind lastKind_;
  DisplayFrame lastFrame_;
  uint32_t beginCalls_;
  bool rotation180_;
  u8g2_t u8g2_;
  std::vector<uint8_t> pixel4bit_;
};

}  // namespace asap::display

#endif  // ARDUINO

