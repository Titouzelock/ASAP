#pragma once

#ifdef ASAP_NATIVE

#include <stdint.h>
#include <vector>
#include <asap/display/DisplayTypes.h>
class U8G2;  // forward declare base class to avoid exposing heavy header here

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

  const DisplayFrame& lastFrame() const;
  FrameKind lastFrameKind() const { return lastKind_; }
  uint32_t beginCount() const { return beginCalls_; }

  void setRotation180(bool enabled);
  bool rotation180() const { return rotation180_; }

  void drawAnomalyIndicators(uint8_t radPercent, uint8_t thermPercent,
                             uint8_t chemPercent, uint8_t psyPercent,
                             uint8_t radStage, uint8_t thermStage,
                             uint8_t chemStage, uint8_t psyStage);

  bool writeSnapshot(const char* filePath) const;  // PGM P5, MaxVal 15

 private:
  void renderFrame(const DisplayFrame& frame, FrameKind kind);
  uint8_t getPixel1bit(uint16_t x, uint16_t y) const;

  DisplayPins pins_;
  U8G2* u8g2_ = nullptr;
  bool initialized_ = false;
  DisplayFrame* lastFramePtr_ = nullptr;
  FrameKind lastKind_ = FrameKind::None;
  uint32_t beginCalls_ = 0;
  bool rotation180_ = false;
};

}  // namespace asap::display

#endif  // ASAP_NATIVE
//
// NativeDisplay.h
// Host-side U8g2 wrapper used by snapshot tests. Owns a plain U8G2 instance
// configured for SSD1322 full-buffer mode and exposes the same API surface as
// the embedded DetectorDisplay. Rendering is delegated to the shared U8g2
// renderer so layouts stay pixel-identical across platforms.
//
// Notes for maintainers
// - This header avoids including U8g2 to keep dependencies light; the .cpp
//   performs the concrete setup with U8g2lib.
// - Snapshot export writes 4-bit PGM (P5, MaxVal 15) decoded from U8g2’s
//   vertical-top buffer layout.
