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
#endif

namespace asap::display {

namespace {

constexpr uint16_t kDisplayWidth = 256;   // SSD1322 canvas width in pixels
constexpr uint16_t kDisplayHeight = 64;   // SSD1322 canvas height in pixels

// Determine the length of a null-terminated string up to maxLen characters.
uint8_t findLength(const char* text, uint8_t maxLen) {
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
void copyText(char* dest, uint8_t maxLen, const char* text) {
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
void appendText(char* dest, uint8_t maxLen, const char* suffix) {
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
void appendNumber(char* dest, uint8_t maxLen, uint32_t value) {
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

constexpr std::array<GlyphDef, 40> kGlyphTable = {{
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}},
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
    {'X', {0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1F}},
    {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
}};

const GlyphDef* findGlyph(char ch) {
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
DisplayFrame makeBootFrame(const char* versionText) {
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;

  DisplayLine& title = frame.lines[frame.lineCount++];
  copyText(title.text, DisplayLine::kMaxLineLength, "ASAP DETECTOR");
  title.font = FontStyle::Title;
  title.y = 26;

  DisplayLine& subtitle = frame.lines[frame.lineCount++];
  copyText(subtitle.text, DisplayLine::kMaxLineLength, "Initializing display...");
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
DisplayFrame makeHeartbeatFrame(uint32_t uptimeMs) {
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = true;
  frame.spinnerIndex =
      static_cast<uint8_t>((uptimeMs / 150U) % 4U);  // four spinner positions

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
DisplayFrame makeStatusFrame(const char* line1, const char* line2) {
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

#ifdef ARDUINO

#include <SPI.h>  // Arduino SPI helpers for the STM32 core

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

bool DetectorDisplay::begin() {
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

  initialized_ = true;
  return true;
}

void DetectorDisplay::drawBootScreen(const char* versionText) {
  if (!initialized_ && !begin()) {
    return;
  }

  lastKind_ = FrameKind::Boot;
  DisplayFrame frame = makeBootFrame(versionText);
  renderFrame(frame);
}

void DetectorDisplay::drawHeartbeatFrame(uint32_t uptimeMs) {
  if (!initialized_) {
    return;
  }

  lastKind_ = FrameKind::Heartbeat;
  DisplayFrame frame = makeHeartbeatFrame(uptimeMs);
  renderFrame(frame);
}

void DetectorDisplay::showStatus(const char* line1, const char* line2) {
  if (!initialized_) {
    return;
  }

  lastKind_ = FrameKind::Status;
  DisplayFrame frame = makeStatusFrame(line1, line2);
  renderFrame(frame);
}

void DetectorDisplay::renderFrame(const DisplayFrame& frame) {
  lastFrame_ = frame;

  u8g2_.clearBuffer();

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

  u8g2_.sendBuffer();
}

void DetectorDisplay::drawSpinner(uint8_t activeIndex,
                                  uint16_t cx,
                                  uint16_t cy) {
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

void DetectorDisplay::drawCentered(const char* text, uint16_t y) {
  if (!text) {
    return;
  }

  const int16_t width = u8g2_.getStrWidth(text);
  const int16_t x =
      static_cast<int16_t>((kDisplayWidth - static_cast<uint16_t>(width)) / 2);
  u8g2_.drawStr(x, y, text);
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

bool DetectorDisplay::begin() {
  ++beginCalls_;
  initialized_ = true;
  return true;
}

void DetectorDisplay::drawBootScreen(const char* versionText) {
  if (!initialized_ && !begin()) {
    return;
  }

  DisplayFrame frame = makeBootFrame(versionText);
  renderFrame(frame, FrameKind::Boot);
}

void DetectorDisplay::drawHeartbeatFrame(uint32_t uptimeMs) {
  if (!initialized_) {
    return;
  }

  DisplayFrame frame = makeHeartbeatFrame(uptimeMs);
  renderFrame(frame, FrameKind::Heartbeat);
}

void DetectorDisplay::showStatus(const char* line1, const char* line2) {
  if (!initialized_) {
    return;
  }

  DisplayFrame frame = makeStatusFrame(line1, line2);
  renderFrame(frame, FrameKind::Status);
}

FrameKind DetectorDisplay::lastFrameKind() const {
  return lastKind_;
}

const DisplayFrame& DetectorDisplay::lastFrame() const {
  return lastFrame_;
}

uint32_t DetectorDisplay::beginCount() const {
  return beginCalls_;
}

void DetectorDisplay::renderFrame(const DisplayFrame& frame, FrameKind kind) {
  lastFrame_ = frame;
  lastKind_ = kind;

  clearBuffer();

  for (uint8_t i = 0; i < frame.lineCount; ++i) {
    const DisplayLine& line = frame.lines[i];
    drawCentered(line.text, line.font, line.y);
  }

  if (frame.spinnerActive) {
    drawSpinner(frame.spinnerIndex, kDisplayWidth / 2U, kDisplayHeight / 2U);
  }
}

void DetectorDisplay::clearBuffer(uint8_t value) {
  std::fill(pixelBuffer_.begin(), pixelBuffer_.end(), value);
}

void DetectorDisplay::drawSpinner(uint8_t activeIndex,
                                  uint16_t cx,
                                  uint16_t cy) {
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
                                   uint16_t y) {
  if (!text) {
    return;
  }

  const uint8_t scale = (font == FontStyle::Title) ? 2 : 1;
  drawText(text, y, scale > 0 ? scale : 1);
}

void DetectorDisplay::drawText(const char* text, uint16_t baseline, uint8_t scale) {
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
                               uint8_t scale) {
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
                                          uint8_t scale) const {
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

void DetectorDisplay::setPixel(int16_t x, int16_t y, uint8_t value) {
  if (x < 0 || y < 0) {
    return;
  }
  if (x >= static_cast<int16_t>(kDisplayWidth) ||
      y >= static_cast<int16_t>(kDisplayHeight)) {
    return;
  }

  const size_t index =
      static_cast<size_t>(y) * static_cast<size_t>(kDisplayWidth) +
      static_cast<size_t>(x);
  if (index < pixelBuffer_.size()) {
    pixelBuffer_[index] = value;
  }
}

bool DetectorDisplay::writeSnapshot(const char* filePath) const {
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

#endif  // ARDUINO

}  // namespace asap::display
