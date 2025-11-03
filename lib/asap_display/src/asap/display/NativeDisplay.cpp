#include "asap/display/NativeDisplay.h"

#ifndef ARDUINO

#include <algorithm>
#include <fstream>
#include <iterator>

#include "asap/display/DisplayTypes.h"
#include "asap/display/assets/AnomalyIcons.h"

extern "C"
{
#include "u8x8.h"
extern const uint8_t u8g2_font_6x10_tr[];
extern const uint8_t u8g2_font_6x13_tr[];
}

namespace asap::display
{

namespace
{

constexpr uint8_t kNibbleMask = 0x0F;

uint8_t byteCallback(u8x8_t*,
                     uint8_t msg,
                     uint8_t arg_int,
                     void* arg_ptr)
{
  switch (msg)
  {
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_START_TRANSFER:
    case U8X8_MSG_BYTE_END_TRANSFER:
      return 1;
    case U8X8_MSG_BYTE_SEND:
      return 1;  // drop all SPI writes
    default:
      (void)arg_int;
      (void)arg_ptr;
      return 1;
  }
}

uint8_t gpioCallback(u8x8_t*,
                     uint8_t msg,
                     uint8_t arg_int,
                     void*)
{
  switch (msg)
  {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
    case U8X8_MSG_GPIO_CS:
    case U8X8_MSG_GPIO_DC:
    case U8X8_MSG_GPIO_RESET:
    case U8X8_MSG_GPIO_I2C_CLOCK:
    case U8X8_MSG_GPIO_I2C_DATA:
      return 1;
    case U8X8_MSG_DELAY_MILLI:
    case U8X8_MSG_DELAY_10MICRO:
    case U8X8_MSG_DELAY_100NANO:
      (void)arg_int;
      return 1;
    default:
      (void)arg_int;
      return 1;
  }
}

const char* romanFor(uint8_t stage)
{
  switch (stage)
  {
    case 1: return "I";
    case 2: return "II";
    case 3: return "III";
    default: return "-";
  }
}

void drawArc(u8g2_t& u8g2,
             int16_t cx,
             int16_t cy,
             uint8_t radius,
             uint8_t thickness,
             uint8_t percent)
{
  const uint16_t steps = static_cast<uint16_t>((percent > 100 ? 100 : percent) * 120U / 100U);
  constexpr int32_t kScale = 1024;
  constexpr int32_t kCos = 1022;
  constexpr int32_t kSin = 53;
  int32_t x = 0;
  int32_t y = -static_cast<int32_t>(radius);
  for (uint16_t i = 0; i <= steps; ++i)
  {
    const int16_t px = static_cast<int16_t>(cx + x);
    const int16_t py = static_cast<int16_t>(cy + y);
    for (int8_t t = -static_cast<int8_t>(thickness) / 2; t <= static_cast<int8_t>(thickness) / 2; ++t)
    {
      u8g2_DrawPixel(&u8g2, px, static_cast<int16_t>(py + t));
    }
    const int32_t nx = (x * kCos - y * kSin + (kScale / 2)) / kScale;
    const int32_t ny = (x * kSin + y * kCos + (kScale / 2)) / kScale;
    x = nx;
    y = ny;
  }
}

}  // namespace

NativeDisplay::NativeDisplay(const DisplayPins& pins)
    : pins_(pins),
      initialized_(false),
      lastKind_(FrameKind::None),
      beginCalls_(0),
      rotation180_(false)
{
  lastFrame_.lineCount = 0;
  lastFrame_.spinnerActive = false;
  lastFrame_.spinnerIndex = 0;
  std::fill(std::begin(lastFrame_.lines), std::end(lastFrame_.lines), DisplayLine{});
  pixel4bit_.reserve((kDisplayWidth * kDisplayHeight) / 2);
}

bool NativeDisplay::begin()
{
  ++beginCalls_;
  if (initialized_)
  {
    return true;
  }

  u8g2_Setup_ssd1322_nhd_256x64_f(&u8g2_,
                                  U8G2_R0,
                                  byteCallback,
                                  gpioCallback);
  u8g2_InitDisplay(&u8g2_);
  u8g2_SetPowerSave(&u8g2_, 0);
  u8g2_SetContrast(&u8g2_, 255);
  u8g2_SetFontMode(&u8g2_, 1);
  u8g2_SetDrawColor(&u8g2_, 1);
  u8g2_SetFontDirection(&u8g2_, 0);
  u8g2_ClearBuffer(&u8g2_);

  if (rotation180_)
  {
    u8g2_SetDisplayRotation(&u8g2_, U8G2_R2);
  }

  pixel4bit_.assign((kDisplayWidth * kDisplayHeight) / 2, 0);
  initialized_ = true;
  return true;
}

void NativeDisplay::drawBootScreen(const char* versionText)
{
  if (!initialized_ && !begin())
  {
    return;
  }

  DisplayFrame frame = makeBootFrame(versionText);
  renderFrame(frame, FrameKind::Boot);
}

void NativeDisplay::drawHeartbeatFrame(uint32_t uptimeMs)
{
  if (!initialized_)
  {
    return;
  }

  DisplayFrame frame = makeHeartbeatFrame(uptimeMs);
  renderFrame(frame, FrameKind::Heartbeat);
}

void NativeDisplay::showStatus(const char* line1, const char* line2)
{
  if (!initialized_)
  {
    return;
  }

  DisplayFrame frame = makeStatusFrame(line1, line2);
  renderFrame(frame, FrameKind::Status);
}

void NativeDisplay::showJoystick(::asap::input::JoyAction action)
{
  if (!initialized_)
  {
    return;
  }
  DisplayFrame frame = makeJoystickFrame(action);
  renderFrame(frame, FrameKind::Status);
}

void NativeDisplay::renderCustom(const DisplayFrame& frame, FrameKind kind)
{
  if (!initialized_)
  {
    return;
  }
  renderFrame(frame, kind);
}

void NativeDisplay::setRotation180(bool enabled)
{
  rotation180_ = enabled;
  if (!initialized_)
  {
    return;
  }

  if (rotation180_)
  {
    u8g2_SetDisplayRotation(&u8g2_, U8G2_R2);
  }
  else
  {
    u8g2_SetDisplayRotation(&u8g2_, U8G2_R0);
  }
}

void NativeDisplay::drawAnomalyIndicators(uint8_t radPercent, uint8_t thermPercent,
                                          uint8_t chemPercent, uint8_t psyPercent,
                                          uint8_t radStage, uint8_t thermStage,
                                          uint8_t chemStage, uint8_t psyStage)
{
  if (!initialized_ && !begin())
  {
    return;
  }

  lastKind_ = FrameKind::MainAnomaly;
  u8g2_ClearBuffer(&u8g2_);

  struct Item
  {
    int16_t cx;
    int16_t cy;
    uint8_t percent;
    uint8_t stage;
    const uint8_t* icon;
  };

  const Item items[4] = {
      {32, 23, radPercent,  radStage,  assets::kIconRadiation30x30},
      {96, 23, thermPercent, thermStage, assets::kIconFire30x30},
      {160, 23, chemPercent,  chemStage,  assets::kIconBiohazard30x30},
      {224, 23, psyPercent,   psyStage,   assets::kIconPsy30x30},
  };

  for (const auto& it : items)
  {
    const uint8_t radius = 21;
    const uint8_t thickness = 3;
    drawArc(u8g2_, it.cx, it.cy, radius, thickness, it.percent);

    const int16_t ix = static_cast<int16_t>(it.cx - assets::kIconW / 2);
    const int16_t iy = static_cast<int16_t>(it.cy - assets::kIconH / 2);
    u8g2_DrawXBMP(&u8g2_, ix, iy, assets::kIconW, assets::kIconH, it.icon);

    u8g2_SetFont(&u8g2_, u8g2_font_6x10_tr);
    const char* roman = romanFor(it.stage);
    const int16_t rw = u8g2_GetStrWidth(&u8g2_, roman);
    u8g2_DrawStr(&u8g2_, static_cast<int16_t>(it.cx - rw / 2), kRomanBaselineY, roman);
  }

  u8g2_SendBuffer(&u8g2_);
  captureBuffer();
}

bool NativeDisplay::writeSnapshot(const char* filePath) const
{
  if (!filePath || filePath[0] == '\0' || pixel4bit_.empty())
  {
    return false;
  }

  std::ofstream out(filePath, std::ios::binary);
  if (!out)
  {
    return false;
  }

  out << "P5\n" << kDisplayWidth << " " << kDisplayHeight << "\n15\n";

  std::vector<uint8_t> expanded(kDisplayWidth * kDisplayHeight, 0);
  for (size_t idx = 0; idx < expanded.size(); ++idx)
  {
    const size_t byteIndex = idx / 2;
    const bool high = (idx % 2) == 0;
    const uint8_t byte = pixel4bit_[byteIndex];
    const uint8_t value = high ? static_cast<uint8_t>((byte >> 4) & kNibbleMask)
                               : static_cast<uint8_t>(byte & kNibbleMask);
    expanded[idx] = value;
  }

  out.write(reinterpret_cast<const char*>(expanded.data()),
            static_cast<std::streamsize>(expanded.size()));
  return static_cast<bool>(out);
}

void NativeDisplay::renderFrame(const DisplayFrame& frame, FrameKind kind)
{
  lastFrame_ = frame;
  lastKind_ = kind;

  u8g2_ClearBuffer(&u8g2_);

  for (uint8_t i = 0; i < frame.lineCount; ++i)
  {
    const DisplayLine& line = frame.lines[i];
    u8g2_SetFont(&u8g2_, u8g2_font_6x10_tr);
    drawCentered(line.text, line.y);
  }

  if (frame.spinnerActive)
  {
    drawSpinner(frame.spinnerIndex, kDisplayWidth / 2U, kDisplayHeight / 2U);
  }

  if (frame.progressBarEnabled)
  {
    drawProgressBar(frame);
  }

  if (frame.showMenuTag)
  {
    drawMenuTag();
  }

  u8g2_SendBuffer(&u8g2_);
  captureBuffer();
}

void NativeDisplay::drawSpinner(uint8_t activeIndex,
                                uint16_t cx,
                                uint16_t cy)
{
  static const int8_t offsets[4][2] = {
      {0, -12},
      {12, 0},
      {0, 12},
      {-12, 0},
  };

  for (uint8_t i = 0; i < 4; ++i)
  {
    const int16_t x = static_cast<int16_t>(cx) + offsets[i][0];
    const int16_t y = static_cast<int16_t>(cy) + offsets[i][1];
    if (i == activeIndex)
    {
      u8g2_DrawDisc(&u8g2_, x, y, 6, U8G2_DRAW_ALL);
    }
    else
    {
      u8g2_DrawCircle(&u8g2_, x, y, 4, U8G2_DRAW_ALL);
    }
  }
}

void NativeDisplay::drawCentered(const char* text, uint16_t y)
{
  if (!text)
  {
    return;
  }

  const int16_t width = u8g2_GetStrWidth(&u8g2_, text);
  const int16_t x =
      static_cast<int16_t>((kDisplayWidth - static_cast<uint16_t>(width)) / 2);
  u8g2_DrawStr(&u8g2_, x, y, text);
}

void NativeDisplay::drawMenuTag()
{
  u8g2_SetFont(&u8g2_, u8g2_font_6x13_tr);
  const char* tag = "MENU";
  const int16_t width = u8g2_GetStrWidth(&u8g2_, tag);
  const int16_t x =
      static_cast<int16_t>(kDisplayWidth - static_cast<uint16_t>(width) - 2);
  const int16_t y = 12;
  u8g2_DrawStr(&u8g2_, x, y, tag);
}

void NativeDisplay::drawProgressBar(const DisplayFrame& frame)
{
  const uint16_t x = frame.progressX;
  const uint16_t y = frame.progressY;
  const uint16_t w = frame.progressWidth;
  const uint16_t h = frame.progressHeight;

  u8g2_DrawFrame(&u8g2_, static_cast<int16_t>(x), static_cast<int16_t>(y),
                 static_cast<int16_t>(w), static_cast<int16_t>(h));
  const uint16_t innerW = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
  const uint16_t fillW =
      static_cast<uint16_t>((static_cast<uint32_t>(innerW) * frame.progressPercent) / 100U);
  if (h > 2 && fillW > 0)
  {
    u8g2_DrawBox(&u8g2_,
                 static_cast<int16_t>(x + 1),
                 static_cast<int16_t>(y + 1),
                 static_cast<int16_t>(fillW),
                 static_cast<int16_t>(h - 2));
  }
}

void NativeDisplay::captureBuffer()
{
  const uint8_t* buffer = u8g2_GetBufferPtr(&u8g2_);
  const uint8_t tileWidth = u8g2_GetBufferTileWidth(&u8g2_);

  std::fill(pixel4bit_.begin(), pixel4bit_.end(), 0);

  for (uint16_t y = 0; y < kDisplayHeight; ++y)
  {
    for (uint16_t x = 0; x < kDisplayWidth; ++x)
    {
      const uint8_t bit = u8x8_capture_get_pixel_1(x, y, const_cast<uint8_t*>(buffer), tileWidth);
      const uint8_t value = bit ? kNibbleMask : 0;
      const uint32_t idx = static_cast<uint32_t>(y) * kDisplayWidth + x;
      const uint32_t byteIndex = idx / 2;
      if ((idx & 1U) == 0U)
      {
        pixel4bit_[byteIndex] =
            static_cast<uint8_t>((value << 4) & 0xF0);
      }
      else
      {
        pixel4bit_[byteIndex] =
            static_cast<uint8_t>(pixel4bit_[byteIndex] | (value & kNibbleMask));
      }
    }
  }
}

}  // namespace asap::display

#endif  // ARDUINO
