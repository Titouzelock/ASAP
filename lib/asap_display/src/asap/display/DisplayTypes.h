#pragma once

#include <stdint.h>

#include <asap/input/Joystick.h>

namespace asap::display
{

enum class FontStyle : uint8_t
{
  Title,
  Body,
};

enum class FrameKind : uint8_t
{
  None,
  Boot,
  Heartbeat,
  Status,
  Menu,
  MainAnomaly,
  MainTracking,
};

struct DisplayLine
{
  static constexpr uint8_t kMaxLineLength = 31;
  char text[kMaxLineLength + 1];
  FontStyle font;
  uint16_t y;
};

struct DisplayFrame
{
  static constexpr uint8_t kMaxLines = 3;
  DisplayLine lines[kMaxLines];
  uint8_t lineCount;
  bool spinnerActive;
  uint8_t spinnerIndex;
  bool showMenuTag;
  bool progressBarEnabled;
  uint16_t progressX;
  uint16_t progressY;
  uint16_t progressWidth;
  uint16_t progressHeight;
  uint8_t progressPercent;
};

struct DisplayPins
{
  uint32_t chipSelect;
  uint32_t dataCommand;
  uint32_t reset;
};

// Factories
DisplayFrame makeBootFrame(const char* versionText);
DisplayFrame makeHeartbeatFrame(uint32_t uptimeMs);
DisplayFrame makeStatusFrame(const char* line1, const char* line2);
DisplayFrame makeJoystickFrame(::asap::input::JoyAction action);
DisplayFrame makeMenuRootFrame(uint8_t selectedIndex);
DisplayFrame makeMenuTrackingFrame(uint8_t trackingId);
DisplayFrame makeMenuAnomalyFrame();
DisplayFrame makeAnomalyMainFrame(uint8_t percent, bool showMenuTag = false);
DisplayFrame makeTrackingMainFrame(uint8_t trackingId,
                                   int16_t rssiAvgDbm,
                                   bool showMenuTag = false);

}  // namespace asap::display
//
// DisplayTypes.h
// Shared UI model for ASAP display rendering across embedded (U8g2 on SSD1322)
// and native host tests. These types describe what to draw (lines, widgets,
// frame kind) without prescribing how to draw it. Platform-specific wrappers
// (DetectorDisplay for embedded, NativeDisplay for host) pass these frames into
// the shared U8g2-based renderer.
//
// Notes for maintainers
// - Keep this header free of platform-specific includes to avoid dependency
//   cycles. It should be safe to include from both embedded and native code.
// - Any additions here should be mirrored in the renderer and (if needed)
//   validated by snapshot tests in the native environment.

