#pragma once

#include <stdint.h>  // fixed-width types shared between MCU and native builds

#ifdef ARDUINO
#include <stdlib.h>   // basic C helpers required by the Arduino core
#include <Arduino.h>  // core Arduino definitions for STM32
#include <U8g2lib.h>  // U8g2 display driver used for the SSD1322 panel
#else
#include <vector>       // host-side buffer capture
#endif

#include <asap/input/Joystick.h>  // JoyAction for joystick debug page

namespace asap::display
{

// Selects which font variant to use when drawing a line.
enum class FontStyle : uint8_t
{
  Title,
  Body,
};

// Identifies which screen/profile was rendered.
// Treat this as a UI state identifier (boot splash, heartbeat HUD, status card).
enum class FrameKind : uint8_t
{
  None,
  Boot,
  Heartbeat,
  Status,
  Menu,         // in-menu navigation frames
  MainAnomaly,  // primary anomaly HUD
  MainTracking, // primary tracking HUD
};

// Represents one line of text with its font and vertical position.
struct DisplayLine
{
  static constexpr uint8_t kMaxLineLength = 31;
  char text[kMaxLineLength + 1];  // zero-terminated copy of the text payload
  FontStyle font;                 // font style for this line
  uint16_t y;                     // baseline y position in pixels
};

// Frame container describing everything needed to render a screen update.
// This struct abstracts the logical page content (text lines, overlays, and
// optional widgets) from the hardware/native drawing implementation.
struct DisplayFrame
{
  static constexpr uint8_t kMaxLines = 3;
  DisplayLine lines[kMaxLines];  // ordered list of text lines to draw
  uint8_t lineCount;             // number of valid entries in lines[]
  bool spinnerActive;            // true when this frame wants an activity hint
  uint8_t spinnerIndex;          // active spinner segment (0..3) when enabled
  // UI affordances
  bool showMenuTag;              // draw "MENU" tag at top-right when true
  // Optional horizontal progress bar (used by Anomaly main page)
  bool progressBarEnabled;       // render a progress bar when true
  uint16_t progressX;            // bar left x
  uint16_t progressY;            // bar top y
  uint16_t progressWidth;        // bar width in pixels
  uint16_t progressHeight;       // bar height in pixels
  uint8_t progressPercent;       // 0..100 fill percentage
};

// Factory helpers for the pre-defined display states.
DisplayFrame makeBootFrame(const char* versionText);
DisplayFrame makeHeartbeatFrame(uint32_t uptimeMs);
DisplayFrame makeStatusFrame(const char* line1, const char* line2);
DisplayFrame makeJoystickFrame(::asap::input::JoyAction action);

// Menu and main-page helpers
DisplayFrame makeMenuRootFrame(uint8_t selectedIndex);
DisplayFrame makeMenuTrackingFrame(uint8_t trackingId);
DisplayFrame makeMenuAnomalyFrame();
DisplayFrame makeAnomalyMainFrame(uint8_t percent, bool showMenuTag = false);
DisplayFrame makeTrackingMainFrame(uint8_t trackingId,
                                   int16_t rssiAvgDbm,
                                   bool showMenuTag = false);

// SPI pin mapping used by the SSD1322 display.
struct DisplayPins
{
  uint32_t chipSelect;   // SPI chip-select GPIO
  uint32_t dataCommand;  // data/command selector GPIO
  uint32_t reset;        // display reset GPIO
};

#ifdef ARDUINO

// Hardware-backed renderer using U8g2 on the STM32 target.
class DetectorDisplay
{
 public:
  explicit DetectorDisplay(const DisplayPins& pins);

  bool begin();                                 // initialise the OLED panel
  void drawBootScreen(const char* versionText); // show boot splash with version
  void drawHeartbeatFrame(uint32_t uptimeMs);   // refresh heartbeat screen
  void showStatus(const char* line1, const char* line2);  // generic status UI
  void showJoystick(::asap::input::JoyAction action);       // joystick debug UI

  // Render a custom frame produced by the factory helpers above.
  void renderCustom(const DisplayFrame& frame, FrameKind kind);

  // Runtime rotation preference (180°). Affects subsequent renders.
  // On embedded (U8g2), this uses setDisplayRotation(R2/R0).
  // On native, the renderer flips pixel coordinates in software.
  void setRotation180(bool enabled);
  bool rotation180() const { return rotation180_; }

  // Anomaly indicators: draw four icons with circular progress and stage.
  // percents: 0..100 for the current partial turn around the icon.
  // stages: 0..3 (displayed as -, I, II, III below the icon).
  void drawAnomalyIndicators(uint8_t radPercent, uint8_t thermPercent,
                             uint8_t chemPercent, uint8_t psyPercent,
                             uint8_t radStage, uint8_t thermStage,
                             uint8_t chemStage, uint8_t psyStage);

  const DisplayFrame& lastFrame() const { return lastFrame_; }  // for debugging
  FrameKind lastFrameKind() const { return lastKind_; }         // track frame type
  uint32_t beginCount() const { return beginCalls_; }           // begin() calls seen

 private:
  void renderFrame(const DisplayFrame& frame);                 // send frame to OLED
  void drawSpinner(uint8_t activeIndex, uint16_t cx, uint16_t cy);  // spinner helper
  void drawCentered(const char* text, uint16_t y);             // center text on x-axis
  void drawMenuTag();                                          // draw top-right label
  void drawProgressBar(const DisplayFrame& frame);             // progress bar helper

  DisplayPins pins_;                                           // cached wiring info
  U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2_;                   // U8g2 driver instance
  bool initialized_;                                          // tracks successful begin()
  DisplayFrame lastFrame_;                                    // last rendered frame record
  FrameKind lastKind_;                                        // last rendered frame type
  uint32_t beginCalls_;                                       // number of begin() invocations
  bool rotation180_ = false;                                  // runtime rotation flag
};

#else

// Native mock implementation that captures frames for testing.
class DetectorDisplay
{
 public:
  explicit DetectorDisplay(const DisplayPins& pins);

  bool begin();                                 // mark the mock as initialised
  void drawBootScreen(const char* versionText); // capture boot frame
  void drawHeartbeatFrame(uint32_t uptimeMs);   // capture heartbeat frame
  void showStatus(const char* line1, const char* line2);  // capture status frame
  void showJoystick(::asap::input::JoyAction action);       // capture joystick frame

  FrameKind lastFrameKind() const;              // expose type of last frame
  const DisplayFrame& lastFrame() const;        // expose captured frame contents
 uint32_t beginCount() const;                  // number of begin() calls observed
  const std::vector<uint8_t>& pixelBuffer() const { return pixelBuffer_; }
  bool writeSnapshot(const char* filePath) const;  // dump buffer to a PGM file

  // Render a custom frame produced by the factory helpers above.
  void renderCustom(const DisplayFrame& frame, FrameKind kind);

  // Runtime rotation preference (180°) for native renderer
  void setRotation180(bool enabled);
  bool rotation180() const { return rotation180_; }

  // Anomaly indicators renderer for native snapshots
  void drawAnomalyIndicators(uint8_t radPercent, uint8_t thermPercent,
                             uint8_t chemPercent, uint8_t psyPercent,
                             uint8_t radStage, uint8_t thermStage,
                             uint8_t chemStage, uint8_t psyStage);

 private:
  void renderFrame(const DisplayFrame& frame, FrameKind kind);
  void clearBuffer(uint8_t value = 0);
  void drawSpinner(uint8_t activeIndex, uint16_t cx, uint16_t cy);
  void drawCentered(const char* text, FontStyle font, uint16_t y);
  void drawText(const char* text, uint16_t y, uint8_t scale);
  void drawChar(char ch, int16_t x, int16_t baseline, uint8_t scale);
  int16_t measureTextWidth(const char* text, uint8_t scale) const;
  void setPixel(int16_t x, int16_t y, uint8_t value);
  void drawMenuTag();
  void drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void drawArc(int16_t cx, int16_t cy, uint8_t radius, uint8_t thickness, uint8_t percent);
  void blitXbm(int16_t x, int16_t y, const uint8_t* bits, uint8_t w, uint8_t h);

  DisplayPins pins_;            // captured wiring (not used but kept for parity)
  bool initialized_;            // mock state flag mirroring hardware path
  FrameKind lastKind_;          // category of the most recent frame
  DisplayFrame lastFrame_;      // cached copy of most recent frame
  uint32_t beginCalls_;         // count of begin() invocations for tests
  std::vector<uint8_t> pixelBuffer_;  // grayscale copy of the current frame
  bool rotation180_ = false;    // runtime rotation flag for mock
};

#endif  // ARDUINO

}  // namespace asap::display
