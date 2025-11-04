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

#if 0  // removed legacy arc helper
static void DrawArcU8g2_old(::U8G2& u8g2,
                        int16_t cx,
                        int16_t cy,
                        uint8_t radius,
                        uint8_t thickness,
                        uint8_t percent)
{
  const uint16_t steps = static_cast<uint16_t>(
      ((percent > 100 ? 100 : percent) * 120U) / 100U); // 3° per step
  constexpr int32_t kScale = 1024;
  constexpr int32_t kCos = 1023; // round(1024*cos(3°))
  constexpr int32_t kSin = 54;   // round(1024*sin(3°))

  int32_t x = 0;
  int32_t y = -static_cast<int32_t>(radius);

  const int16_t half = static_cast<int16_t>(thickness) / 2;
  const int16_t rMin = static_cast<int16_t>(radius) - half;
  const int16_t rMax = static_cast<int16_t>(radius) + (static_cast<int16_t>(thickness) - 1 - half);

  for (uint16_t i = 0; i <= steps; ++i)
  {
    for (int16_t r = rMin; r <= rMax; ++r)
    {
      const int16_t px = static_cast<int16_t>(cx + (x * r + (radius / 2)) / radius);
      const int16_t py = static_cast<int16_t>(cy + (y * r + (radius / 2)) / radius);
      u8g2.drawPixel(px, py);
    }

    // Rotate 3° clockwise (Y grows downward)
    const int32_t nx = (x * kCos + y * kSin + (kScale / 2)) / kScale;
    const int32_t ny = (-x * kSin + y * kCos + (kScale / 2)) / kScale;
    x = nx;
    y = ny;
  }
}
#endif  // removed legacy arc helper

// New LUT-driven implementation (integer-only) replacing the runtime-rotation version.
static void DrawArcU8g2(::U8G2& u8g2,
                        int16_t cx,
                        int16_t cy,
                        uint8_t radius,
                        uint8_t thickness,
                        uint8_t percent)
{
  // One-quadrant sine table (0..90° inclusive) with 65 samples (~1.40625°/step),
  // fixed-point scaled by 1024. Full circle reconstructed by mirroring across quadrants.
  constexpr int32_t kScale = 1024;
  static const int16_t kSinLut[65] = {
      0,   25,   50,   75,  100,  125,  150,  175,  200,  224,
    249,  273,  297,  321,  345,  369,  392,  415,  438,  460,
    483,  505,  526,  548,  569,  590,  610,  630,  650,  669,
    688,  706,  724,  742,  759,  775,  792,  807,  822,  837,
    851,  865,  878,  891,  903,  915,  926,  936,  946,  955,
    964,  972,  980,  987,  993,  999, 1004, 1009, 1013, 1016,
   1019, 1021, 1023, 1024, 1024
  };

  // Percent to discrete steps: 4 quadrants * 64 steps = 256 steps per full circle.
  constexpr uint16_t kStepsPerQuadrant = 64;
  constexpr uint16_t kFullSteps = kStepsPerQuadrant * 4U;
  const uint8_t pc = (percent > 100) ? 100 : percent;
  const uint16_t stepsToDraw = static_cast<uint16_t>((static_cast<uint32_t>(pc) * kFullSteps) / 100U);

  // Radial thickness bounds around base radius (inclusive)
  const int16_t half = static_cast<int16_t>(thickness) / 2;
  const int16_t rMin = static_cast<int16_t>(radius) - half;
  const int16_t rMax = static_cast<int16_t>(radius) + (static_cast<int16_t>(thickness) - 1 - half);

  for (uint16_t s = 0; s <= stepsToDraw; ++s)
  {
    const uint16_t q = static_cast<uint16_t>(s / kStepsPerQuadrant);   // 0..3
    const uint16_t w = static_cast<uint16_t>(s % kStepsPerQuadrant);   // 0..63

    const int16_t sin_w = kSinLut[w];
    const int16_t cos_w = kSinLut[kStepsPerQuadrant - w];

    int32_t ux = 0;
    int32_t uy = 0;
    switch (q & 3U)
    {
      case 0:  // 0..90°: right, up
        ux =  sin_w;
        uy = -cos_w;
        break;
      case 1:  // 90..180°: right, down
        ux =  cos_w;
        uy =  sin_w;
        break;
      case 2:  // 180..270°: left, down
        ux = -sin_w;
        uy =  cos_w;
        break;
      default: // 270..360°: left, up
        ux = -cos_w;
        uy = -sin_w;
        break;
    }

    for (int16_t r = rMin; r <= rMax; ++r)
    {
      const int16_t px = static_cast<int16_t>(cx + static_cast<int16_t>((ux * r + (kScale / 2)) / kScale));
      const int16_t py = static_cast<int16_t>(cy + static_cast<int16_t>((uy * r + (kScale / 2)) / kScale));
      u8g2.drawPixel(px, py);
    }
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
    // Draw the icon first so subsequent arc pixels can overlay it.
    const int16_t ix = static_cast<int16_t>(it.cx - assets::kIconW / 2);
    const int16_t iy = static_cast<int16_t>(it.cy - assets::kIconH / 2);
    const uint8_t* bits = assets::kIconRadiation30x30;
    if (it.icon[0] == 'T') bits = assets::kIconFire30x30;
    else if (it.icon[0] == 'C') bits = assets::kIconBiohazard30x30;
    else if (it.icon[0] == 'P') bits = assets::kIconPsy30x30;
    u8g2.drawXBMP(ix, iy, assets::kIconW, assets::kIconH, bits);
    // Draw the arc after the icon so the ring has the final say.
    DrawArcU8g2(u8g2, it.cx, it.cy, radius, thick, it.p);

    u8g2.setFont(u8g2_font_6x10_tr);
    const char* roman = RomanFor(it.s);
    const int16_t rw = u8g2.getStrWidth(roman);
    u8g2.drawStr(static_cast<int16_t>(it.cx - rw / 2), kRomanBaselineY, roman);
  }
}

}  // namespace asap::display
//
// DisplayRenderer.cpp
// Implementation of the shared U8g2 rendering path. This translates the
// platform-neutral DisplayFrame (lines, tags, progress, HUD elements) into
// concrete U8g2 draw calls. Both STM32 (hardware) and the native host use
// these helpers to maintain rendering parity and snapshot stability.
//
// Rendering guarantees
// - Text centering uses U8g2 string width for consistent alignment.
// - Anomaly HUD geometry (icon positions, arc radius/thickness, roman baseline)
//   is kept consistent with embedded requirements documented in AGENTS.md.
// - Avoid platform-specific conditionals here; divergence should live in
//   wrapper classes that own U8g2 instances (DetectorDisplay/NativeDisplay).
