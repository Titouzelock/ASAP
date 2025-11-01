#pragma once

#include <stdint.h>  // fixed-width types shared between MCU and native builds

#ifdef ARDUINO
#include <stdlib.h>   // basic C helpers required by the Arduino core
#include <Arduino.h>  // core Arduino definitions for STM32
#include <U8g2lib.h>  // U8g2 display driver used for the SSD1322 panel
#else
#include <vector>       // host-side buffer capture
#endif

namespace asap::display {

// Selects which font variant to use when drawing a line.
enum class FontStyle : uint8_t {
  Title,
  Body,
};

// Identifies the semantic type of the most recent frame.
enum class FrameKind : uint8_t {
  None,
  Boot,
  Heartbeat,
  Status,
};

// Represents one line of text with its font and vertical position.
struct DisplayLine {
  static constexpr uint8_t kMaxLineLength = 31;
  char text[kMaxLineLength + 1];  // zero-terminated copy of the text payload
  FontStyle font;                 // font style for this line
  uint16_t y;                     // baseline y position in pixels
};

// Frame container describing everything needed to render a screen update.
struct DisplayFrame {
  static constexpr uint8_t kMaxLines = 3;
  DisplayLine lines[kMaxLines];  // ordered list of text lines to draw
  uint8_t lineCount;             // number of valid entries in lines[]
  bool spinnerActive;            // true when the heartbeat spinner should show
  uint8_t spinnerIndex;          // active spinner segment (0..3)
};

// Factory helpers for the pre-defined display states.
DisplayFrame makeBootFrame(const char* versionText);
DisplayFrame makeHeartbeatFrame(uint32_t uptimeMs);
DisplayFrame makeStatusFrame(const char* line1, const char* line2);

// SPI pin mapping used by the SSD1322 display.
struct DisplayPins {
  uint32_t chipSelect;   // SPI chip-select GPIO
  uint32_t dataCommand;  // data/command selector GPIO
  uint32_t reset;        // display reset GPIO
};

#ifdef ARDUINO

// Hardware-backed renderer using U8g2 on the STM32 target.
class DetectorDisplay {
 public:
  explicit DetectorDisplay(const DisplayPins& pins);

  bool begin();                                 // initialise the OLED panel
  void drawBootScreen(const char* versionText); // show boot splash with version
  void drawHeartbeatFrame(uint32_t uptimeMs);   // refresh heartbeat screen
  void showStatus(const char* line1, const char* line2);  // generic status UI

  const DisplayFrame& lastFrame() const { return lastFrame_; }  // for debugging
  FrameKind lastFrameKind() const { return lastKind_; }         // track frame type
  uint32_t beginCount() const { return beginCalls_; }           // begin() calls seen

 private:
  void renderFrame(const DisplayFrame& frame);                 // send frame to OLED
  void drawSpinner(uint8_t activeIndex, uint16_t cx, uint16_t cy);  // spinner helper
  void drawCentered(const char* text, uint16_t y);             // center text on x-axis

  DisplayPins pins_;                                           // cached wiring info
  U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2_;                   // U8g2 driver instance
  bool initialized_;                                          // tracks successful begin()
  DisplayFrame lastFrame_;                                    // last rendered frame record
  FrameKind lastKind_;                                        // last rendered frame type
  uint32_t beginCalls_;                                       // number of begin() invocations
};

#else

// Native mock implementation that captures frames for testing.
class DetectorDisplay {
 public:
  explicit DetectorDisplay(const DisplayPins& pins);

  bool begin();                                 // mark the mock as initialised
  void drawBootScreen(const char* versionText); // capture boot frame
  void drawHeartbeatFrame(uint32_t uptimeMs);   // capture heartbeat frame
  void showStatus(const char* line1, const char* line2);  // capture status frame

  FrameKind lastFrameKind() const;              // expose type of last frame
  const DisplayFrame& lastFrame() const;        // expose captured frame contents
 uint32_t beginCount() const;                  // number of begin() calls observed
  const std::vector<uint8_t>& pixelBuffer() const { return pixelBuffer_; }
  bool writeSnapshot(const char* filePath) const;  // dump buffer to a PGM file

 private:
  void renderFrame(const DisplayFrame& frame, FrameKind kind);
  void clearBuffer(uint8_t value = 0);
  void drawSpinner(uint8_t activeIndex, uint16_t cx, uint16_t cy);
  void drawCentered(const char* text, FontStyle font, uint16_t y);
  void drawText(const char* text, uint16_t y, uint8_t scale);
  void drawChar(char ch, int16_t x, int16_t baseline, uint8_t scale);
  int16_t measureTextWidth(const char* text, uint8_t scale) const;
  void setPixel(int16_t x, int16_t y, uint8_t value);

  DisplayPins pins_;            // captured wiring (not used but kept for parity)
  bool initialized_;            // mock state flag mirroring hardware path
  FrameKind lastKind_;          // category of the most recent frame
  DisplayFrame lastFrame_;      // cached copy of most recent frame
  uint32_t beginCalls_;         // count of begin() invocations for tests
  std::vector<uint8_t> pixelBuffer_;  // grayscale copy of the current frame
};

#endif  // ARDUINO

}  // namespace asap::display
