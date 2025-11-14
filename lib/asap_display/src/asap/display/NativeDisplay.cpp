#ifdef ASAP_NATIVE

#include <asap/display/NativeDisplay.h>
#include <asap/display/DisplayRenderer.h>
#include <asap/display/DetectorDisplay.h>

// Native-target U8g2 integration: we construct a plain U8G2 and
// configure it for the SSD1322 full buffer. The Arduino byte/gpio
// callbacks are provided as no-ops in U8g2NativeStubs.cpp so we can
// render entirely in-memory on the host and export snapshots.
#include <U8g2lib.h>
#include <u8x8.h>

// Declare Arduino helper from U8x8lib.cpp (C++ linkage)
void u8x8_SetPin_4Wire_HW_SPI(u8x8_t* u8x8, uint8_t cs, uint8_t dc, uint8_t reset);
#include <fstream>

namespace asap::display
{

namespace
{
constexpr uint16_t kDisplayWidth = 256;
constexpr uint16_t kDisplayHeight = 64;
}

NativeDisplay::NativeDisplay(const DisplayPins& pins)
    : pins_(pins)
{
  lastFramePtr_ = new DisplayFrame{};
  lastFramePtr_->lineCount = 0;
  lastFramePtr_->spinnerActive = false;
  lastFramePtr_->spinnerIndex = 0;
}

bool NativeDisplay::begin()
{
  ++beginCalls_;
  if (initialized_)
  {
    return true;
  }
  // Lazily construct and configure U8g2 for SSD1322 full-buffer mode.
  if (!u8g2_)
  {
    u8g2_ = new ::U8G2();
    u8g2_Setup_ssd1322_nhd_256x64_f(u8g2_->getU8g2(), U8G2_R0,
                                     u8x8_byte_arduino_hw_spi,
                                     u8x8_gpio_and_delay_arduino);
    // No pin mapping required on native; GPIO callback is a no-op
  }
  // Drive the in-memory buffer; nothing is sent to hardware on native.
  u8g2_->begin();
  u8g2_->setContrast(255);
  u8g2_->setFontMode(1);
  u8g2_->setDrawColor(1);
  u8g2_->setFontDirection(0);
  u8g2_->clearBuffer();
  if (rotation180_)
  {
    u8g2_->setDisplayRotation(U8G2_R2);
  }
  initialized_ = true;
  return true;
}

void NativeDisplay::drawBootScreen(const char* versionText)
{
  if (!initialized_ && !begin())
  {
    return;
  }
  lastKind_ = FrameKind::Boot;
  DisplayFrame frame = makeBootFrame(versionText);
  renderFrame(frame, lastKind_);
}

void NativeDisplay::drawHeartbeatFrame(uint32_t uptimeMs)
{
  if (!initialized_)
  {
    return;
  }
  lastKind_ = FrameKind::Heartbeat;
  DisplayFrame frame = makeHeartbeatFrame(uptimeMs);
  renderFrame(frame, lastKind_);
}

void NativeDisplay::showStatus(const char* line1, const char* line2)
{
  if (!initialized_)
  {
    return;
  }
  lastKind_ = FrameKind::Status;
  DisplayFrame frame = makeStatusFrame(line1, line2);
  renderFrame(frame, lastKind_);
}

void NativeDisplay::showJoystick(::asap::input::JoyAction action)
{
  if (!initialized_)
  {
    return;
  }
  lastKind_ = FrameKind::Status;
  DisplayFrame frame = makeJoystickFrame(action);
  renderFrame(frame, lastKind_);
}

void NativeDisplay::renderCustom(const DisplayFrame& frame, FrameKind kind)
{
  if (!initialized_)
  {
    return;
  }
  lastKind_ = kind;
  renderFrame(frame, lastKind_);
}

void NativeDisplay::setRotation180(bool enabled)
{
  rotation180_ = enabled;
  if (initialized_)
  {
    if (rotation180_)
    {
    u8g2_->setDisplayRotation(U8G2_R2);
    }
    else
    {
      u8g2_->setDisplayRotation(U8G2_R0);
    }
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
  drawAnomalyIndicatorsU8g2(*u8g2_, radPercent, thermPercent, chemPercent, psyPercent,
                            radStage, thermStage, chemStage, psyStage);
}

void NativeDisplay::renderFrame(const DisplayFrame& frame, FrameKind kind)
{
  (void)kind;
  if (lastFramePtr_)
  {
    *lastFramePtr_ = frame;
  }
  renderFrameU8g2(*u8g2_, frame);
}

uint8_t NativeDisplay::getPixel1bit(uint16_t x, uint16_t y) const
{
  // U8g2’s SSD1322 full buffer is in “vertical-top” layout:
  //  - Bytes are organized in strips of 8 vertical pixels.
  //  - For (x,y): group = y/8, within = y%8, index = group*bytesPerRow + x.
  //  - The bit for the pixel is (byte >> within)&1.
  // This matches the library’s own capture helpers and avoids scrambled output.
  if (!u8g2_)
    return 0;
  const uint8_t* buf = u8g2_->getBufferPtr();
  const uint16_t tileWidthTiles = u8g2_->getBufferTileWidth();
  const uint32_t bytesPerRow = static_cast<uint32_t>(tileWidthTiles) * 8u;
  const uint32_t group = static_cast<uint32_t>(y) >> 3;  // y / 8
  const uint8_t within = static_cast<uint8_t>(y & 7u);
  const uint32_t index = group * bytesPerRow + static_cast<uint32_t>(x);
  const uint8_t byte = buf[index];
  const uint8_t mask = static_cast<uint8_t>(1u << within);
  return (byte & mask) ? 1u : 0u;
}

bool NativeDisplay::writeSnapshot(const char* filePath) const
{
  if (!filePath || !initialized_)
  {
    return false;
  }
  std::ofstream out(filePath, std::ios::binary);
  if (!out)
  {
    return false;
  }
  out << "P5\n" << kDisplayWidth << " " << kDisplayHeight << "\n15\n";

  for (uint16_t y = 0; y < kDisplayHeight; ++y)
  {
    for (uint16_t x = 0; x < kDisplayWidth; ++x)
    {
      const uint8_t bit = getPixel1bit(x, y);
      const uint8_t val = static_cast<uint8_t>(bit ? 15 : 0);
      out.put(static_cast<char>(val));
    }
  }
  return static_cast<bool>(out);
}

const DisplayFrame& NativeDisplay::lastFrame() const
{
  return *lastFramePtr_;
}

}  // namespace asap::display

extern "C" void asap_display_native_anchor() {}

#endif  // ASAP_NATIVE
//
// NativeDisplay.cpp
// Host implementation of the ASAP display using U8g2’s full buffer for the
// SSD1322 controller. This constructs a base U8G2, configures it with no-op
// Arduino callbacks, and renders via shared helpers. The buffer is decoded as
// vertical-top for 4-bit PGM snapshot export to mirror hardware.
//
