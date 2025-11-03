#include "asap/display/DetectorDisplay.h"

#include <stddef.h>  // size_t for helper routines

#ifndef ARDUINO
#include <algorithm>  // std::fill
#include <array>      // static glyph definitions
#include <cctype>     // std::toupper
#include <cmath>      // std::abs
#include <cstring>    // std::strlen
#include <fstream>    // writing snapshot images
#include <string>     // text normalization
#include "asap/display/assets/AnomalyIcons.h"
#endif

namespace asap::display
{

namespace
{

constexpr uint16_t kDisplayWidth = 256;   // SSD1322 canvas width in pixels
constexpr uint16_t kDisplayHeight = 64;   // SSD1322 canvas height in pixels

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

#ifndef ARDUINO
constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;

struct GlyphDef {
  char ch;
  std::array<uint8_t, kGlyphHeight> rows;
};

// 5x7 ASCII glyphs used by the native renderer. Add entries as UI grows.
constexpr std::array<GlyphDef, 44> kGlyphTable = {{
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}},
    {'>', {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01}},  // menu cursor caret
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},  // minus sign for RSSI
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x1C}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}},
    {'X', {0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1F}},
    {'J', {0x1F, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}},
    {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
}};

const GlyphDef* findGlyph(char ch)
{
  for (const auto& glyph : kGlyphTable) {
    if (glyph.ch == ch) {
      return &glyph;
    }
  }
  return nullptr;
}
#endif

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
DisplayFrame makeJoystickFrame(asap::input::JoyAction action)
{
  const char* word = "NEUTRAL";
  switch (action)
  {
    case asap::input::JoyAction::Left: word = "LEFT"; break;
    case asap::input::JoyAction::Right: word = "RIGHT"; break;
    case asap::input::JoyAction::Up: word = "UP"; break;
    case asap::input::JoyAction::Down: word = "DOWN"; break;
    case asap::input::JoyAction::Click: word = "CLICK"; break;
    case asap::input::JoyAction::Neutral: default: word = "NEUTRAL"; break;
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

void DetectorDisplay::showJoystick(asap::input::JoyAction action)
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

  u8g2_.clearBuffer();

  // Iterate each logical line and translate into concrete font calls.
  for (uint8_t i = 0; i < frame.lineCount; ++i) {
    const DisplayLine& line = frame.lines[i];
    if (line.font == FontStyle::Title) {
      u8g2_.setFont(u8g2_font_ncenB14_tr);
    } else {
      u8g2_.setFont(u8g2_font_6x13_tr);
    }
    drawCentered(line.text, line.y);
  }

  if (frame.spinnerActive) {
    drawSpinner(frame.spinnerIndex, kDisplayWidth / 2U, kDisplayHeight / 2U);
  }

  if (frame.progressBarEnabled) {
    drawProgressBar(frame);
  }

  if (frame.showMenuTag) {
    drawMenuTag();
  }

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

// Helper: draw a ring arc (0..100%) around center
static void DrawArcU8g2(U8G2& u8g2,
                        int16_t cx,
                        int16_t cy,
                        uint8_t radius,
                        uint8_t thickness,
                        uint8_t percent)
{
  const float twoPi = 6.2831853f;
  const float end = (percent > 100 ? 100 : percent) * twoPi / 100.0f;
  const float step = 3.0f * 3.1415926f / 180.0f;  // ~3 degrees
  for (float a = 0.0f; a <= end; a += step)
  {
    const int16_t x = static_cast<int16_t>(cx + radius * std::cos(a));
    const int16_t y = static_cast<int16_t>(cy + radius * std::sin(a));
    for (int8_t t = -static_cast<int8_t>(thickness) / 2; t <= static_cast<int8_t>(thickness) / 2; ++t)
    {
      u8g2.drawPixel(x, static_cast<int16_t>(y + t));
    }
  }
}

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
  u8g2_.clearBuffer();

  struct Item { int16_t cx, cy; uint8_t p; uint8_t s; const char* icon; };
  const Item items[4] = {
      {32, 28, radPercent,  radStage,  "RAD"},
      {96, 28, thermPercent, thermStage, "THERM"},
      {160,28, chemPercent,  chemStage,  "CHEM"},
      {224,28, psyPercent,   psyStage,  "PSY"},
  };

  for (const auto& it : items)
  {
    // Outer ring and progress
    const uint8_t radius = 18; // surrounds 30x30 icon with margin
    const uint8_t thick = 3;
    u8g2_.drawCircle(it.cx, it.cy, radius);
    DrawArcU8g2(u8g2_, it.cx, it.cy, radius, thick, it.p);

    // Icon: XBM asset centered (16x16 placeholder)
    const int16_t ix = static_cast<int16_t>(it.cx - asap::display::assets::kIconW / 2);
    const int16_t iy = static_cast<int16_t>(it.cy - asap::display::assets::kIconH / 2);
    const uint8_t* bits = asap::display::assets::kIconRadiation30x30;
    if (it.icon[0] == 'T') bits = asap::display::assets::kIconFire30x30;
    else if (it.icon[0] == 'C') bits = asap::display::assets::kIconBiohazard30x30;
    else if (it.icon[0] == 'P') bits = asap::display::assets::kIconPsy30x30;
    u8g2_.drawXBMP(ix, iy, asap::display::assets::kIconW, asap::display::assets::kIconH, bits);

    // Stage label centered below ring
    u8g2_.setFont(u8g2_font_6x10_tr);
    const char* roman = RomanFor(it.s);
    const int16_t rw = u8g2_.getStrWidth(roman);
    u8g2_.drawStr(static_cast<int16_t>(it.cx - rw / 2), static_cast<int16_t>(it.cy + radius + 8), roman);
  }

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

#else  // !ARDUINO

DetectorDisplay::DetectorDisplay(const DisplayPins& pins)
    : pins_(pins),
      initialized_(false),
      lastKind_(FrameKind::None),
      beginCalls_(0),
      pixelBuffer_(kDisplayWidth * kDisplayHeight, 0) {
  lastFrame_.lineCount = 0;
  lastFrame_.spinnerActive = false;
  lastFrame_.spinnerIndex = 0;
}

bool DetectorDisplay::begin()
{
  ++beginCalls_;
  initialized_ = true;
  return true;
}

void DetectorDisplay::drawBootScreen(const char* versionText)
{
  if (!initialized_ && !begin()) {
    return;
  }

  DisplayFrame frame = makeBootFrame(versionText);
  renderFrame(frame, FrameKind::Boot);
}

void DetectorDisplay::drawHeartbeatFrame(uint32_t uptimeMs)
{
  if (!initialized_) {
    return;
  }

  DisplayFrame frame = makeHeartbeatFrame(uptimeMs);
  renderFrame(frame, FrameKind::Heartbeat);
}

void DetectorDisplay::showStatus(const char* line1, const char* line2)
{
  if (!initialized_) {
    return;
  }

  DisplayFrame frame = makeStatusFrame(line1, line2);
  renderFrame(frame, FrameKind::Status);
}

void DetectorDisplay::showJoystick(asap::input::JoyAction action)
{
  if (!initialized_) {
    return;
  }
  DisplayFrame frame = makeJoystickFrame(action);
  renderFrame(frame, FrameKind::Status);
}

void DetectorDisplay::renderCustom(const DisplayFrame& frame, FrameKind kind)
{
  if (!initialized_) {
    return;
  }
  renderFrame(frame, kind);
}

FrameKind DetectorDisplay::lastFrameKind() const
{
  return lastKind_;
}

const DisplayFrame& DetectorDisplay::lastFrame() const
{
  return lastFrame_;
}

uint32_t DetectorDisplay::beginCount() const
{
  return beginCalls_;
}

void DetectorDisplay::renderFrame(const DisplayFrame& frame, FrameKind kind)
{
  lastFrame_ = frame;
  lastKind_ = kind;

  // The native mock mirrors the hardware renderer but draws into an in-memory
  // grayscale buffer instead of the OLED controller.
  clearBuffer();

  for (uint8_t i = 0; i < frame.lineCount; ++i) {
    const DisplayLine& line = frame.lines[i];
    drawCentered(line.text, line.font, line.y);
  }

  if (frame.spinnerActive) {
    drawSpinner(frame.spinnerIndex, kDisplayWidth / 2U, kDisplayHeight / 2U);
  }

  if (frame.progressBarEnabled) {
    // border
    drawRect(frame.progressX, frame.progressY, frame.progressWidth, frame.progressHeight);
    // inner fill
    if (frame.progressWidth > 2 && frame.progressHeight > 2 && frame.progressPercent > 0) {
      const uint16_t innerW = static_cast<uint16_t>(frame.progressWidth - 2);
      const uint16_t fillW = static_cast<uint16_t>((static_cast<uint32_t>(innerW) * frame.progressPercent) / 100U);
      if (fillW > 0) {
        fillRect(static_cast<uint16_t>(frame.progressX + 1),
                 static_cast<uint16_t>(frame.progressY + 1),
                 fillW,
                 static_cast<uint16_t>(frame.progressHeight - 2));
      }
    }
  }

  if (frame.showMenuTag) {
    drawMenuTag();
  }
}

void DetectorDisplay::clearBuffer(uint8_t value)
{
  std::fill(pixelBuffer_.begin(), pixelBuffer_.end(), value);
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

  const int filledRadius = 6;
  const int hollowRadius = 4;

  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t centerX = static_cast<int16_t>(cx) + offsets[i][0];
    const int16_t centerY = static_cast<int16_t>(cy) + offsets[i][1];
    const int radius = (i == activeIndex) ? filledRadius : hollowRadius;
    const bool filled = (i == activeIndex);

    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        const int dist2 = dx * dx + dy * dy;
        const int r2 = radius * radius;
        if (filled) {
          if (dist2 <= r2) {
            setPixel(centerX + dx, centerY + dy, 255);
          }
        } else {
          if (std::abs(dist2 - r2) <= radius) {
            setPixel(centerX + dx, centerY + dy, 255);
          }
        }
      }
    }
  }
}

void DetectorDisplay::drawCentered(const char* text,
                                   FontStyle font,
                                   uint16_t y)
{
  if (!text) {
    return;
  }

  const uint8_t scale = (font == FontStyle::Title) ? 2 : 1;
  drawText(text, y, scale > 0 ? scale : 1);
}

void DetectorDisplay::drawText(const char* text, uint16_t baseline, uint8_t scale)
{
  if (!text || scale == 0) {
    return;
  }

  std::string normalized;
  normalized.reserve(std::strlen(text));
  for (const char* ptr = text; *ptr != '\0'; ++ptr) {
    char ch = *ptr;
    if (ch == '\n' || ch == '\r') {
      continue;
    }
    if ('a' <= ch && ch <= 'z') {
      ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    normalized.push_back(ch);
  }

  if (normalized.empty()) {
    return;
  }

  const int16_t advance = static_cast<int16_t>(kGlyphWidth * scale + scale);
  const int16_t width =
      static_cast<int16_t>(advance * static_cast<int16_t>(normalized.size()) -
                           scale);
  int16_t cursorX = static_cast<int16_t>((kDisplayWidth - width) / 2);

  for (size_t i = 0; i < normalized.size(); ++i) {
    char ch = normalized[i];
    drawChar(ch, cursorX, static_cast<int16_t>(baseline), scale);
    cursorX += advance;
  }
}

void DetectorDisplay::drawChar(char ch,
                               int16_t x,
                               int16_t baseline,
                               uint8_t scale)
{
  const GlyphDef* glyph = findGlyph(ch);
  if (!glyph) {
    glyph = findGlyph('?');
    if (!glyph) {
      return;
    }
  }

  const int16_t top =
      baseline - static_cast<int16_t>(kGlyphHeight * scale) + 1;

  for (int row = 0; row < kGlyphHeight; ++row) {
    const uint8_t bits = glyph->rows[row];
    for (int col = 0; col < kGlyphWidth; ++col) {
      const bool set = (bits >> (kGlyphWidth - 1 - col)) & 0x01;
      if (!set) {
        continue;
      }
      for (uint8_t dy = 0; dy < scale; ++dy) {
        for (uint8_t dx = 0; dx < scale; ++dx) {
          const int16_t px = x + static_cast<int16_t>(col * scale + dx);
          const int16_t py =
              top + static_cast<int16_t>(row * scale + dy);
          setPixel(px, py, 255);
        }
      }
    }
  }
}

int16_t DetectorDisplay::measureTextWidth(const char* text,
                                          uint8_t scale) const
{
  if (!text || scale == 0) {
    return 0;
  }

  int count = 0;
  for (const char* ptr = text; *ptr != '\0'; ++ptr) {
    const char ch = *ptr;
    if (ch == '\n' || ch == '\r') {
      continue;
    }
    ++count;
  }

  if (count == 0) {
    return 0;
  }

  const int16_t advance = static_cast<int16_t>(kGlyphWidth * scale + scale);
  return static_cast<int16_t>(advance * count - scale);
}

void DetectorDisplay::setPixel(int16_t x, int16_t y, uint8_t value)
{
  if (x < 0 || y < 0) {
    return;
  }
  if (x >= static_cast<int16_t>(kDisplayWidth) ||
      y >= static_cast<int16_t>(kDisplayHeight)) {
    return;
  }

  // Apply runtime 180° rotation by mapping coordinates.
  // Hardware path uses U8g2 rotation; the native path emulates it in software.
  int16_t rx = x;
  int16_t ry = y;
  if (rotation180_)
  {
    rx = static_cast<int16_t>(kDisplayWidth - 1 - x);
    ry = static_cast<int16_t>(kDisplayHeight - 1 - y);
  }

  const size_t index =
      static_cast<size_t>(ry) * static_cast<size_t>(kDisplayWidth) +
      static_cast<size_t>(rx);
  if (index < pixelBuffer_.size()) {
    pixelBuffer_[index] = value;
  }
}

void DetectorDisplay::drawMenuTag()
{
  const char* tag = "MENU";
  const uint8_t scale = 2;  // readable but compact
  const int16_t width = measureTextWidth(tag, scale);
  const int16_t x = static_cast<int16_t>(kDisplayWidth - width - 2);
  const int16_t y = 14;  // near the top
  // drawText centers around x by default; here we want left-aligned at x.
  // Implement a simple left-aligned renderer: reuse drawChar loop.
  std::string normalized(tag);
  const int16_t advance = static_cast<int16_t>(kGlyphWidth * scale + scale);
  int16_t cursorX = x;
  const int16_t baseline = y;
  for (char ch : normalized) {
    drawChar(ch, cursorX, baseline, scale);
    cursorX += advance;
  }
}

void DetectorDisplay::drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  if (w == 0 || h == 0) return;
  for (uint16_t i = 0; i < w; ++i) {
    setPixel(static_cast<int16_t>(x + i), static_cast<int16_t>(y), 255);
    setPixel(static_cast<int16_t>(x + i), static_cast<int16_t>(y + h - 1), 255);
  }
  for (uint16_t j = 0; j < h; ++j) {
    setPixel(static_cast<int16_t>(x), static_cast<int16_t>(y + j), 255);
    setPixel(static_cast<int16_t>(x + w - 1), static_cast<int16_t>(y + j), 255);
  }
}

void DetectorDisplay::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  for (uint16_t j = 0; j < h; ++j) {
    for (uint16_t i = 0; i < w; ++i) {
      setPixel(static_cast<int16_t>(x + i), static_cast<int16_t>(y + j), 255);
    }
  }
}

bool DetectorDisplay::writeSnapshot(const char* filePath) const
{
  if (!filePath || filePath[0] == '\0' || pixelBuffer_.empty()) {
    return false;
  }

  std::ofstream out(filePath, std::ios::binary);
  if (!out) {
    return false;
  }

  out << "P5\n" << kDisplayWidth << " " << kDisplayHeight << "\n255\n";
  out.write(reinterpret_cast<const char*>(pixelBuffer_.data()),
            static_cast<std::streamsize>(pixelBuffer_.size()));
  return static_cast<bool>(out);
}

void DetectorDisplay::setRotation180(bool enabled)
{
  rotation180_ = enabled;
}

void DetectorDisplay::drawArc(int16_t cx,
                              int16_t cy,
                              uint8_t radius,
                              uint8_t thickness,
                              uint8_t percent)
{
  const float twoPi = 6.2831853f;
  const float end = (percent > 100 ? 100 : percent) * twoPi / 100.0f;
  const float step = 3.0f * 3.1415926f / 180.0f;  // ~3 degrees
  for (float a = 0.0f; a <= end; a += step)
  {
    const int16_t x = static_cast<int16_t>(cx + radius * std::cos(a));
    const int16_t y = static_cast<int16_t>(cy + radius * std::sin(a));
    for (int8_t t = -static_cast<int8_t>(thickness) / 2; t <= static_cast<int8_t>(thickness) / 2; ++t)
    {
      setPixel(x, static_cast<int16_t>(y + t), 255);
    }
  }
}

void DetectorDisplay::blitXbm(int16_t x, int16_t y,
                              const uint8_t* bits, uint8_t w, uint8_t h)
{
  const uint8_t bytesPerRow = static_cast<uint8_t>((w + 7) / 8);
  for (uint8_t row = 0; row < h; ++row)
  {
    for (uint8_t col = 0; col < w; ++col)
    {
      const uint8_t byte = bits[row * bytesPerRow + (col >> 3)];
      const uint8_t mask = static_cast<uint8_t>(1u << (col & 7)); // XBM LSB first
      if (static_cast<uint8_t>(byte & mask) != 0)
      {
        setPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), 255);
      }
    }
  }
}

void DetectorDisplay::drawAnomalyIndicators(uint8_t radPercent, uint8_t thermPercent,
                                            uint8_t chemPercent, uint8_t psyPercent,
                                            uint8_t radStage, uint8_t thermStage,
                                            uint8_t chemStage, uint8_t psyStage)
{
  if (!initialized_ && !begin())
  {
    return;
  }
  clearBuffer();
  lastKind_ = FrameKind::MainAnomaly;

  struct Item { int16_t cx, cy; uint8_t p; uint8_t s; const char* label; };
  const Item items[4] = {
      {32, 28, radPercent,  radStage,  "R"},
      {96, 28, thermPercent, thermStage, "F"},
      {160,28, chemPercent,  chemStage,  "B"},
      {224,28, psyPercent,   psyStage,  "PSY"},
  };

  for (const auto& it : items)
  {
    const uint8_t radius = 18;
    const uint8_t thick = 3;
    // Progress arc + full ring hint
    drawArc(it.cx, it.cy, radius, 1, 100);
    drawArc(it.cx, it.cy, radius, thick, it.p);

    // Icon: XBM asset centered (40x40 placeholder in native)
    const int16_t ix = static_cast<int16_t>(it.cx - asap::display::assets::kIconW / 2);
    const int16_t iy = static_cast<int16_t>(it.cy - asap::display::assets::kIconH / 2);
    const uint8_t* bits = asap::display::assets::kIconRadiation30x30;
    if (it.label[0] == 'F') bits = asap::display::assets::kIconFire30x30;
    else if (it.label[0] == 'B') bits = asap::display::assets::kIconBiohazard30x30;
    else if (it.label[0] == 'P') bits = asap::display::assets::kIconPsy30x30;
    blitXbm(ix, iy, bits, asap::display::assets::kIconW, asap::display::assets::kIconH);

    // Stage label centered below ring
    const char* roman = (it.s == 1) ? "I" : (it.s == 2) ? "II" : (it.s == 3) ? "III" : "-";
    drawCentered(roman, FontStyle::Body, static_cast<uint16_t>(it.cy + radius + 8));
  }
}

#endif  // ARDUINO

}  // namespace asap::display
