#include <asap/display/DisplayRenderer.h>

#include <asap/display/DisplayTypes.h>
#include <asap/display/assets/AnomalyIcons.h>

namespace asap::display
{

namespace
{

constexpr uint16_t kDisplayWidth = 256;
constexpr uint16_t kDisplayHeight = 64;
constexpr int16_t kRomanBaselineY = 57;

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

static void DrawArcU8g2(::U8G2& u8g2,
                        int16_t cx,
                        int16_t cy,
                        uint8_t radius,
                        uint8_t thickness,
                        uint8_t percent)
{
  const uint16_t steps = static_cast<uint16_t>((percent > 100 ? 100 : percent) * 120U / 100U);
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
    const int32_t nx = (x * kCos - y * kSin + (kScale / 2)) / kScale;
    const int32_t ny = (x * kSin + y * kCos + (kScale / 2)) / kScale;
    x = nx;
    y = ny;
  }
}

static void drawCentered(::U8G2& u8g2, const char* text, uint16_t y)
{
  if (!text)
  {
    return;
  }

  const int16_t width = u8g2.getStrWidth(text);
  const int16_t x = static_cast<int16_t>((kDisplayWidth - static_cast<uint16_t>(width)) / 2);
  u8g2.drawStr(x, y, text);
}

static void drawSpinner(::U8G2& u8g2, uint8_t activeIndex, uint16_t cx, uint16_t cy)
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
      u8g2.drawDisc(x, y, 6);
    }
    else
    {
      u8g2.drawCircle(x, y, 4);
    }
  }
}

static void drawMenuTag(::U8G2& u8g2)
{
  u8g2.setFont(u8g2_font_6x13_tr);
  const char* tag = "MENU";
  const int16_t width = u8g2.getStrWidth(tag);
  const int16_t x = static_cast<int16_t>(kDisplayWidth - static_cast<uint16_t>(width) - 2);
  const int16_t y = 12;
  u8g2.drawStr(x, y, tag);
}

static void drawProgressBar(::U8G2& u8g2, const DisplayFrame& frame)
{
  const uint16_t x = frame.progressX;
  const uint16_t y = frame.progressY;
  const uint16_t w = frame.progressWidth;
  const uint16_t h = frame.progressHeight;
  u8g2.drawFrame(static_cast<int16_t>(x), static_cast<int16_t>(y),
                 static_cast<int16_t>(w), static_cast<int16_t>(h));
  const uint16_t innerW = (w > 2) ? static_cast<uint16_t>(w - 2) : 0;
  const uint16_t fillW = static_cast<uint16_t>((static_cast<uint32_t>(innerW) * frame.progressPercent) / 100U);
  if (h > 2 && fillW > 0)
  {
    u8g2.drawBox(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                 static_cast<int16_t>(fillW), static_cast<int16_t>(h - 2));
  }
}

}  // namespace

void renderFrameU8g2(::U8G2& u8g2, const DisplayFrame& frame)
{
  u8g2.clearBuffer();

  for (uint8_t i = 0; i < frame.lineCount; ++i)
  {
    const DisplayLine& line = frame.lines[i];
    u8g2.setFont(u8g2_font_6x10_tr);
    drawCentered(u8g2, line.text, line.y);
  }

  if (frame.spinnerActive)
  {
    drawSpinner(u8g2, frame.spinnerIndex, kDisplayWidth / 2U, kDisplayHeight / 2U);
  }

  if (frame.progressBarEnabled)
  {
    drawProgressBar(u8g2, frame);
  }

  if (frame.showMenuTag)
  {
    drawMenuTag(u8g2);
  }
}

void drawAnomalyIndicatorsU8g2(::U8G2& u8g2,
                               uint8_t radPercent, uint8_t thermPercent,
                               uint8_t chemPercent, uint8_t psyPercent,
                               uint8_t radStage, uint8_t thermStage,
                               uint8_t chemStage, uint8_t psyStage)
{
  u8g2.clearBuffer();

  struct Item { int16_t cx, cy; uint8_t p; uint8_t s; const char* icon; };
  const Item items[4] = {
      {32, 23, radPercent,  radStage,  "RAD"},
      {96, 23, thermPercent, thermStage, "THERM"},
      {160,23, chemPercent,  chemStage,  "CHEM"},
      {224,23, psyPercent,   psyStage,  "PSY"},
  };

  for (const auto& it : items)
  {
    const uint8_t radius = 21;
    const uint8_t thick = 3;
    DrawArcU8g2(u8g2, it.cx, it.cy, radius, thick, it.p);

    const int16_t ix = static_cast<int16_t>(it.cx - assets::kIconW / 2);
    const int16_t iy = static_cast<int16_t>(it.cy - assets::kIconH / 2);
    const uint8_t* bits = assets::kIconRadiation30x30;
    if (it.icon[0] == 'T') bits = assets::kIconFire30x30;
    else if (it.icon[0] == 'C') bits = assets::kIconBiohazard30x30;
    else if (it.icon[0] == 'P') bits = assets::kIconPsy30x30;
    u8g2.drawXBMP(ix, iy, assets::kIconW, assets::kIconH, bits);

    u8g2.setFont(u8g2_font_6x10_tr);
    const char* roman = RomanFor(it.s);
    const int16_t rw = u8g2.getStrWidth(roman);
    u8g2.drawStr(static_cast<int16_t>(it.cx - rw / 2), kRomanBaselineY, roman);
  }
}

}  // namespace asap::display
