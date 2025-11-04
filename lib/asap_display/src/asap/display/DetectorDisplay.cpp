#include "asap/display/DetectorDisplay.h"
#include "asap/display/DisplayRenderer.h"

#include <stddef.h>  // size_t for helper routines



namespace asap::display
{

namespace
{

constexpr uint16_t kDisplayWidth = 256;   // SSD1322 canvas width in pixels
constexpr uint16_t kDisplayHeight = 64;   // SSD1322 canvas height in pixels
// Fixed baseline for roman numerals under anomaly indicators to keep their
// screen position stable regardless of icon/circle vertical adjustments.
constexpr int16_t kRomanBaselineY = 57;

// Determine the length of a null-terminated string up to maxLen characters.
uint8_t findLength(const char* text, uint8_t maxLen)
{
  if (!text) {
    return 0;
  }

  uint8_t length = 0;
  while (length < maxLen && text[length] != '\0') {
    ++length;
  }
  return length;
}

// Copy text into a destination buffer clamped to maxLen characters.
void copyText(char* dest, uint8_t maxLen, const char* text)
{
  uint8_t index = 0;
  if (text) {
    while (text[index] != '\0' && index < maxLen) {
      dest[index] = text[index];
      ++index;
    }
  }
  dest[index] = '\0';
}

// Append a suffix string to the destination buffer with length protection.
void appendText(char* dest, uint8_t maxLen, const char* suffix)
{
  uint8_t length = findLength(dest, maxLen);
  if (!suffix) {
    dest[length] = '\0';
    return;
  }

  uint8_t index = 0;
  while (suffix[index] != '\0' && length < maxLen) {
    dest[length++] = suffix[index++];
  }
  dest[length] = '\0';
}

// Append a positive integer to the destination buffer as decimal digits.
void appendNumber(char* dest, uint8_t maxLen, uint32_t value)
{
  uint8_t length = findLength(dest, maxLen);

  char digits[10];       // enough for 32-bit integer
  uint8_t digitCount = 0;
  do {
    digits[digitCount++] = static_cast<char>('0' + (value % 10U));
    value /= 10U;
  } while (value > 0 && digitCount < sizeof(digits));

  while (digitCount > 0 && length < maxLen) {
    dest[length++] = digits[--digitCount];
  }
  dest[length] = '\0';
}



}  // namespace

// Build the boot splash frame with title, subtitle, and optional FW version.
// Compose the boot splash – static branding + optional firmware string.
DisplayFrame makeBootFrame(const char* versionText)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;

  DisplayLine& title = frame.lines[frame.lineCount++];
  copyText(title.text, DisplayLine::kMaxLineLength, "ASAP DETECTOR");
  title.font = FontStyle::Title;
  title.y = 26;

  DisplayLine& subtitle = frame.lines[frame.lineCount++];
  copyText(subtitle.text, DisplayLine::kMaxLineLength, "Titoozelock");
  subtitle.font = FontStyle::Body;
  subtitle.y = 44;

  if (versionText && versionText[0] != '\0' &&
      frame.lineCount < DisplayFrame::kMaxLines) {
    DisplayLine& footer = frame.lines[frame.lineCount++];
    copyText(footer.text, DisplayLine::kMaxLineLength, "FW ");
    appendText(footer.text, DisplayLine::kMaxLineLength, versionText);
    footer.font = FontStyle::Body;
    footer.y = 60;
  }

  return frame;
}

// Build the heartbeat frame presenting uptime and spinner state.
// Compose the idling / heartbeat page shown while the detector is ready.
DisplayFrame makeHeartbeatFrame(uint32_t uptimeMs)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;

  DisplayLine& headline = frame.lines[frame.lineCount++];
  copyText(headline.text, DisplayLine::kMaxLineLength, "Detector ready");
  headline.font = FontStyle::Body;
  headline.y = 20;

  DisplayLine& uptimeLine = frame.lines[frame.lineCount++];
  copyText(uptimeLine.text, DisplayLine::kMaxLineLength, "Uptime ");
  const uint32_t seconds = uptimeMs / 1000U;
  appendNumber(uptimeLine.text, DisplayLine::kMaxLineLength, seconds);
  appendText(uptimeLine.text, DisplayLine::kMaxLineLength, "s");
  uptimeLine.font = FontStyle::Body;
  uptimeLine.y = 52;

  return frame;
}

// Build a generic status frame with up to two lines of text.
// Generic two-line status card (used for RF link states, alerts, etc.).
DisplayFrame makeStatusFrame(const char* line1, const char* line2)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;

  if (line1 && line1[0] != '\0') {
    DisplayLine& first = frame.lines[frame.lineCount++];
    copyText(first.text, DisplayLine::kMaxLineLength, line1);
    first.font = FontStyle::Body;
    first.y = 28;
  }

  if (line2 && line2[0] != '\0' &&
      frame.lineCount < DisplayFrame::kMaxLines) {
    DisplayLine& second = frame.lines[frame.lineCount++];
    copyText(second.text, DisplayLine::kMaxLineLength, line2);
    second.font = FontStyle::Body;
    second.y = 48;
  }

  return frame;
}

// Build a single-word joystick debug frame.
// Build a single-word joystick debug frame.
// Note: use fully-qualified JoyAction values in switch to avoid nested namespace
// lookup issues under ARDUINO builds (asap::display scope).
DisplayFrame makeJoystickFrame(::asap::input::JoyAction action)
{
  const char* word = "NEUTRAL";
  switch (action)
  {
    case ::asap::input::JoyAction::Left:    word = "LEFT";   break;
    case ::asap::input::JoyAction::Right:   word = "RIGHT";  break;
    case ::asap::input::JoyAction::Up:      word = "UP";     break;
    case ::asap::input::JoyAction::Down:    word = "DOWN";   break;
    case ::asap::input::JoyAction::Click:   word = "CLICK";  break;
    case ::asap::input::JoyAction::Neutral: default:         word = "NEUTRAL"; break;
  }

  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;

  DisplayLine& line = frame.lines[frame.lineCount++];
  copyText(line.text, DisplayLine::kMaxLineLength, word);
  line.font = FontStyle::Title;
  line.y = 40;  // vertically pleasing for a single title line
  return frame;
}

// Root menu with three items and a selection caret ('>').
DisplayFrame makeMenuRootFrame(uint8_t selectedIndex)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = true;

  const char* items[3] = {"ANOMALY", "TRACKING", "CONFIG"};
  const uint16_t ys[3] = {20, 38, 56};
  for (uint8_t i = 0; i < 3 && frame.lineCount < DisplayFrame::kMaxLines; ++i) {
    DisplayLine& line = frame.lines[frame.lineCount++];
    // Prefix selected item with caret '>' for clarity in both renderers
    if (i == selectedIndex) {
      copyText(line.text, DisplayLine::kMaxLineLength, "> ");
    } else {
      copyText(line.text, DisplayLine::kMaxLineLength, "  ");
    }
    appendText(line.text, DisplayLine::kMaxLineLength, items[i]);
    line.font = FontStyle::Body;
    line.y = ys[i];
  }
  return frame;
}

// Tracking menu page: adjust ID with LEFT/RIGHT, Click to confirm.
DisplayFrame makeMenuTrackingFrame(uint8_t trackingId)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = true;

  DisplayLine& title = frame.lines[frame.lineCount++];
  copyText(title.text, DisplayLine::kMaxLineLength, "TRACKING");
  title.font = FontStyle::Body;
  title.y = 20;

  DisplayLine& idLine = frame.lines[frame.lineCount++];
  copyText(idLine.text, DisplayLine::kMaxLineLength, "ID ");
  // format 3-digit id
  char idBuf[4];
  idBuf[0] = static_cast<char>('0' + (trackingId / 100U) % 10U);
  idBuf[1] = static_cast<char>('0' + (trackingId / 10U) % 10U);
  idBuf[2] = static_cast<char>('0' + (trackingId % 10U));
  idBuf[3] = '\0';
  appendText(idLine.text, DisplayLine::kMaxLineLength, idBuf);
  idLine.font = FontStyle::Body;
  idLine.y = 38;

  DisplayLine& hint = frame.lines[frame.lineCount++];
  // Avoid '=' since the tiny glyph set may not include it; keep text clean
  copyText(hint.text, DisplayLine::kMaxLineLength, "CLICK OK");
  hint.font = FontStyle::Body;
  hint.y = 56;
  return frame;
}

// Anomaly menu page: simple confirmation to switch main mode.
DisplayFrame makeMenuAnomalyFrame()
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = true;

  DisplayLine& title = frame.lines[frame.lineCount++];
  copyText(title.text, DisplayLine::kMaxLineLength, "ANOMALY");
  title.font = FontStyle::Body;
  title.y = 28;

  DisplayLine& hint = frame.lines[frame.lineCount++];
  // Avoid '=' since the tiny glyph set may not include it; keep text clean
  copyText(hint.text, DisplayLine::kMaxLineLength, "CLICK OK");
  hint.font = FontStyle::Body;
  hint.y = 48;
  return frame;
}

// Anomaly main page with 15px-tall bar across the width, offset from bottom.
DisplayFrame makeAnomalyMainFrame(uint8_t percent, bool showMenuTag)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = showMenuTag;
  frame.progressBarEnabled = true;
  frame.progressX = 0;
  frame.progressWidth = kDisplayWidth;
  frame.progressHeight = 15;
  frame.progressY = static_cast<uint16_t>(kDisplayHeight - frame.progressHeight - 1);
  frame.progressPercent = percent;
  return frame;
}

// Tracking main page: show ID and averaged RSSI; no bar for now.
DisplayFrame makeTrackingMainFrame(uint8_t trackingId,
                                   int16_t rssiAvgDbm,
                                   bool showMenuTag)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = showMenuTag;

  DisplayLine& line1 = frame.lines[frame.lineCount++];
  copyText(line1.text, DisplayLine::kMaxLineLength, "TRACK ");
  appendNumber(line1.text, DisplayLine::kMaxLineLength, trackingId);
  line1.font = FontStyle::Body;
  line1.y = 20;

  DisplayLine& line2 = frame.lines[frame.lineCount++];
  copyText(line2.text, DisplayLine::kMaxLineLength, "RSSI ");
  // format signed RSSI like -85dBm
  if (rssiAvgDbm < 0) {
    appendText(line2.text, DisplayLine::kMaxLineLength, "-");
    appendNumber(line2.text, DisplayLine::kMaxLineLength,
                 static_cast<uint32_t>(-rssiAvgDbm));
  } else {
    appendNumber(line2.text, DisplayLine::kMaxLineLength,
                 static_cast<uint32_t>(rssiAvgDbm));
  }
  appendText(line2.text, DisplayLine::kMaxLineLength, "dBm");
  line2.font = FontStyle::Body;
  line2.y = 52;
  return frame;
}

#ifdef ARDUINO

#include <SPI.h>  // Arduino SPI helpers for the STM32 core
#include "asap/display/assets/AnomalyIcons.h"

DetectorDisplay::DetectorDisplay(const DisplayPins& pins)
    : pins_(pins),
      u8g2_(U8G2_R0,
            static_cast<uint8_t>(pins_.chipSelect),
            static_cast<uint8_t>(pins_.dataCommand),
            static_cast<uint8_t>(pins_.reset)),
      initialized_(false),
      lastKind_(FrameKind::None),
      beginCalls_(0) {
  lastFrame_.lineCount = 0;
  lastFrame_.spinnerActive = false;
  lastFrame_.spinnerIndex = 0;
}

bool DetectorDisplay::begin()
{
  ++beginCalls_;
  if (initialized_) {
    return true;
  }

  u8g2_.begin();
  u8g2_.setContrast(255);
  u8g2_.setFontMode(1);
  u8g2_.setDrawColor(1);
  u8g2_.setFontDirection(0);
  u8g2_.clearBuffer();

  // Apply runtime rotation preference if set
  if (rotation180_)
  {
    u8g2_.setDisplayRotation(U8G2_R2);
  }

  initialized_ = true;
  return true;
}

void DetectorDisplay::drawBootScreen(const char* versionText)
{
  if (!initialized_ && !begin()) {
    return;
  }

  lastKind_ = FrameKind::Boot;
  DisplayFrame frame = makeBootFrame(versionText);
  renderFrame(frame);
}

void DetectorDisplay::drawHeartbeatFrame(uint32_t uptimeMs)
{
  if (!initialized_) {
    return;
  }

  lastKind_ = FrameKind::Heartbeat;
  DisplayFrame frame = makeHeartbeatFrame(uptimeMs);
  renderFrame(frame);
}

void DetectorDisplay::showStatus(const char* line1, const char* line2)
{
  if (!initialized_) {
    return;
  }

  lastKind_ = FrameKind::Status;
  DisplayFrame frame = makeStatusFrame(line1, line2);
  renderFrame(frame);
}

void DetectorDisplay::showJoystick(::asap::input::JoyAction action)
{
  if (!initialized_) {
    return;
  }
  lastKind_ = FrameKind::Status;
  DisplayFrame frame = makeJoystickFrame(action);
  renderFrame(frame);
}

void DetectorDisplay::renderCustom(const DisplayFrame& frame, FrameKind kind)
{
  if (!initialized_) {
    return;
  }
  lastKind_ = kind;
  renderFrame(frame);
}

void DetectorDisplay::renderFrame(const DisplayFrame& frame)
{
  lastFrame_ = frame;
  renderFrameU8g2(u8g2_, frame);
  u8g2_.sendBuffer();
}

void DetectorDisplay::drawSpinner(uint8_t activeIndex,
                                  uint16_t cx,
                                  uint16_t cy)
{
  static const int8_t offsets[4][2] = {
      {0, -12},
      {12, 0},
      {0, 12},
      {-12, 0},
  };

  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t x = static_cast<int16_t>(cx) + offsets[i][0];
    const int16_t y = static_cast<int16_t>(cy) + offsets[i][1];
    if (i == activeIndex) {
      u8g2_.drawDisc(x, y, 6);
    } else {
      u8g2_.drawCircle(x, y, 4);
    }
  }
}

void DetectorDisplay::drawCentered(const char* text, uint16_t y)
{
  if (!text) {
    return;
  }

  const int16_t width = u8g2_.getStrWidth(text);
  const int16_t x =
      static_cast<int16_t>((kDisplayWidth - static_cast<uint16_t>(width)) / 2);
  u8g2_.drawStr(x, y, text);
}

void DetectorDisplay::drawMenuTag()
{
  u8g2_.setFont(u8g2_font_6x13_tr);
  const char* tag = "MENU";
  const int16_t width = u8g2_.getStrWidth(tag);
  const int16_t x = static_cast<int16_t>(kDisplayWidth - static_cast<uint16_t>(width) - 2);
  const int16_t y = 12;  // top area
  u8g2_.drawStr(x, y, tag);
}

void DetectorDisplay::setRotation180(bool enabled)
{
  rotation180_ = enabled;
  if (!initialized_)
  {
    return;
  }
  // U8g2 supports runtime rotation changes; switch immediately
  if (rotation180_)
  {
    u8g2_.setDisplayRotation(U8G2_R2);
  }
  else
  {
    u8g2_.setDisplayRotation(U8G2_R0);
  }
}

// Helper: Roman numeral from stage 0..3
static const char* RomanFor(uint8_t stage)
{
  switch (stage)
  {
    case 1: return "I";
    case 2: return "II";
    case 3: return "III";
    default: return "-";
  }
}

#if 0  // removed: use shared DrawArcU8g2 in DisplayRenderer
// Helper: draw a ring arc (0..100%) around center.
// Starts at north (top) and sweeps clockwise with constant thickness.
static void DrawArcU8g2(U8G2& u8g2,
                        int16_t cx,
                        int16_t cy,
                        uint8_t radius,
                        uint8_t thickness,
                        uint8_t percent)
{
  const uint16_t steps = static_cast<uint16_t>((percent > 100 ? 100 : percent) * 120U / 100U); // 3° per step
  // Fixed-point rotation: start at north (0,-r), apply 3° increments
  constexpr int32_t kScale = 1024;
  constexpr int32_t kCos = 1022; // round(1024*cos(3°))
  constexpr int32_t kSin = 53;   // round(1024*sin(3°))
  int32_t x = 0;
  int32_t y = -static_cast<int32_t>(radius);
  for (uint16_t i = 0; i <= steps; ++i)
  {
    const int16_t px = static_cast<int16_t>(cx + x);
    const int16_t py = static_cast<int16_t>(cy + y);
    for (int8_t t = -static_cast<int8_t>(thickness) / 2; t <= static_cast<int8_t>(thickness) / 2; ++t)
    {
      u8g2.drawPixel(px, static_cast<int16_t>(py + t));
    }
    // rotate by +3° clockwise
    const int32_t nx = (x * kCos - y * kSin + (kScale / 2)) / kScale;
    const int32_t ny = (x * kSin + y * kCos + (kScale / 2)) / kScale;
    x = nx;
    y = ny;
  }
}
#endif  // removed DrawArcU8g2 local in ARDUINO path

void DetectorDisplay::drawAnomalyIndicators(uint8_t radPercent, uint8_t thermPercent,
                                            uint8_t chemPercent, uint8_t psyPercent,
                                            uint8_t radStage, uint8_t thermStage,
                                            uint8_t chemStage, uint8_t psyStage)
{
  if (!initialized_ && !begin())
  {
    return;
  }
  lastKind_ = FrameKind::MainAnomaly;
  drawAnomalyIndicatorsU8g2(u8g2_, radPercent, thermPercent, chemPercent, psyPercent,
                            radStage, thermStage, chemStage, psyStage);
  u8g2_.sendBuffer();
}

void DetectorDisplay::drawProgressBar(const DisplayFrame& frame)
{
  const uint16_t x = frame.progressX;
  const uint16_t y = frame.progressY;
  const uint16_t w = frame.progressWidth;
  const uint16_t h = frame.progressHeight;
  // Border
  u8g2_.drawFrame(static_cast<int16_t>(x), static_cast<int16_t>(y),
                  static_cast<int16_t>(w), static_cast<int16_t>(h));
  // Fill inside border
  const uint16_t innerW = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
  const uint16_t fillW = static_cast<uint16_t>((static_cast<uint32_t>(innerW) * frame.progressPercent) / 100U);
  if (h > 2 && fillW > 0) {
    u8g2_.drawBox(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                  static_cast<int16_t>(fillW), static_cast<int16_t>(h - 2));
  }
}

#endif  // ARDUINO

}  // namespace asap::display



