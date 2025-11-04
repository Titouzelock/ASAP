#include "asap/display/DetectorDisplay.h"

#ifdef ARDUINO

#include <SPI.h>

#include "asap/display/DisplayTypes.h"
#include "asap/display/assets/AnomalyIcons.h"

namespace asap::display
{

namespace
{

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

void drawArc(U8G2& u8g2,
             int16_t cx,
             int16_t cy,
             uint8_t radius,
             uint8_t thickness,
             uint8_t percent)
{
  const uint8_t clamped = (percent > 100) ? 100 : percent;
  uint16_t steps = static_cast<uint16_t>((clamped * 360U) / 100U);
  if (steps == 0 && clamped > 0)
  {
    steps = 1;
  }
  constexpr int32_t kScale = 16384;
  constexpr int32_t kCos = 16362;  // round(16384 * cos(1°))
  constexpr int32_t kSin = 286;    // round(16384 * sin(1°))
  int32_t x = 0;
  int32_t y = -static_cast<int32_t>(radius) * kScale;
  for (uint16_t i = 0; i <= steps; ++i)
  {
    const int16_t px = static_cast<int16_t>(cx + (x + (kScale/2)) / kScale);
    const int16_t py = static_cast<int16_t>(cy + (y + (kScale/2)) / kScale);
    for (int8_t t = -static_cast<int8_t>(thickness) / 2; t <= static_cast<int8_t>(thickness) / 2; ++t)
    {
      u8g2.drawPixel(px, static_cast<int16_t>(py + t));
    }
    const int32_t nx = (x * kCos - y * kSin + (kScale / 2)) / kScale;
    const int32_t ny = (x * kSin + y * kCos + (kScale / 2)) / kScale;
    x = nx;
    y = ny;
  }
}

}  // namespace

DetectorDisplay::DetectorDisplay(const DisplayPins& pins)
    : pins_(pins),
      u8g2_(U8G2_R0,
            static_cast<uint8_t>(pins_.chipSelect),
            static_cast<uint8_t>(pins_.dataCommand),
            static_cast<uint8_t>(pins_.reset)),
      initialized_(false),
      lastKind_(FrameKind::None),
      beginCalls_(0)
{
  lastFrame_.lineCount = 0;
  lastFrame_.spinnerActive = false;
  lastFrame_.spinnerIndex = 0;
}

bool DetectorDisplay::begin()
{
  ++beginCalls_;
  if (initialized_)
  {
    return true;
  }

  u8g2_.begin();
  u8g2_.setContrast(255);
  u8g2_.setFontMode(1);
  u8g2_.setDrawColor(1);
  u8g2_.setFontDirection(0);
  u8g2_.clearBuffer();

  if (rotation180_)
  {
    u8g2_.setDisplayRotation(U8G2_R2);
  }

  initialized_ = true;
  return true;
}

void DetectorDisplay::drawBootScreen(const char* versionText)
{
  if (!initialized_ && !begin())
  {
    return;
  }

  lastKind_ = FrameKind::Boot;
  DisplayFrame frame = makeBootFrame(versionText);
  renderFrame(frame);
}

void DetectorDisplay::drawHeartbeatFrame(uint32_t uptimeMs)
{
  if (!initialized_)
  {
    return;
  }

  lastKind_ = FrameKind::Heartbeat;
  DisplayFrame frame = makeHeartbeatFrame(uptimeMs);
  renderFrame(frame);
}

void DetectorDisplay::showStatus(const char* line1, const char* line2)
{
  if (!initialized_)
  {
    return;
  }

  lastKind_ = FrameKind::Status;
  DisplayFrame frame = makeStatusFrame(line1, line2);
  renderFrame(frame);
}

void DetectorDisplay::showJoystick(::asap::input::JoyAction action)
{
  if (!initialized_)
  {
    return;
  }

  lastKind_ = FrameKind::Status;
  DisplayFrame frame = makeJoystickFrame(action);
  renderFrame(frame);
}

void DetectorDisplay::renderCustom(const DisplayFrame& frame, FrameKind kind)
{
  if (!initialized_)
  {
    return;
  }
  lastKind_ = kind;
  renderFrame(frame);
}

void DetectorDisplay::renderFrame(const DisplayFrame& frame)
{
  lastFrame_ = frame;
  u8g2_.clearBuffer();

  for (uint8_t i = 0; i < frame.lineCount; ++i)
  {
    const DisplayLine& line = frame.lines[i];
    u8g2_.setFont(u8g2_font_6x10_tr);
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

  for (uint8_t i = 0; i < 4; ++i)
  {
    const int16_t x = static_cast<int16_t>(cx) + offsets[i][0];
    const int16_t y = static_cast<int16_t>(cy) + offsets[i][1];
    if (i == activeIndex)
    {
      u8g2_.drawDisc(x, y, 6);
    }
    else
    {
      u8g2_.drawCircle(x, y, 4);
    }
  }
}

void DetectorDisplay::drawCentered(const char* text, uint16_t y)
{
  if (!text)
  {
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
  const int16_t x =
      static_cast<int16_t>(kDisplayWidth - static_cast<uint16_t>(width) - 2);
  const int16_t y = 12;
  u8g2_.drawStr(x, y, tag);
}

void DetectorDisplay::setRotation180(bool enabled)
{
  rotation180_ = enabled;
  if (!initialized_)
  {
    return;
  }
  if (rotation180_)
  {
    u8g2_.setDisplayRotation(U8G2_R2);
  }
  else
  {
    u8g2_.setDisplayRotation(U8G2_R0);
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
    u8g2_.drawCircle(it.cx, it.cy, radius);
    drawArc(u8g2_, it.cx, it.cy, radius, thickness, it.percent);

    const int16_t ix = static_cast<int16_t>(it.cx - assets::kIconW / 2);
    const int16_t iy = static_cast<int16_t>(it.cy - assets::kIconH / 2);
    u8g2_.drawXBMP(ix, iy, assets::kIconW, assets::kIconH, it.icon);

    u8g2_.setFont(u8g2_font_6x10_tr);
    const char* roman = romanFor(it.stage);
    const int16_t rw = u8g2_.getStrWidth(roman);
    u8g2_.drawStr(static_cast<int16_t>(it.cx - rw / 2), kRomanBaselineY, roman);
  }

  u8g2_.sendBuffer();
}

void DetectorDisplay::drawProgressBar(const DisplayFrame& frame)
{
  const uint16_t x = frame.progressX;
  const uint16_t y = frame.progressY;
  const uint16_t w = frame.progressWidth;
  const uint16_t h = frame.progressHeight;

  u8g2_.drawFrame(static_cast<int16_t>(x), static_cast<int16_t>(y),
                  static_cast<int16_t>(w), static_cast<int16_t>(h));
  const uint16_t innerW = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
  const uint16_t fillW =
      static_cast<uint16_t>((static_cast<uint32_t>(innerW) * frame.progressPercent) / 100U);
  if (h > 2 && fillW > 0)
  {
    u8g2_.drawBox(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                  static_cast<int16_t>(fillW), static_cast<int16_t>(h - 2));
  }
}

}  // namespace asap::display

#endif  // ARDUINO
