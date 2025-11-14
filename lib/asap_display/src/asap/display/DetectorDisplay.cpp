#include "asap/display/DetectorDisplay.h"
#include "asap/display/DisplayRenderer.h"

#include <stddef.h>

#ifdef ASAP_EMBEDDED
extern "C"
{
#include "stm32f1xx_hal.h"
#include "main.h"
#include <u8x8.h>
}
#endif

namespace asap::display
{

namespace
{

// Helpers shared by frame factories
uint8_t findLength(const char* text, uint8_t maxLen)
{
  if (!text)
  {
    return 0;
  }

  uint8_t length = 0;
  while (length < maxLen && text[length] != '\0')
  {
    ++length;
  }
  return length;
}

void copyText(char* dest, uint8_t maxLen, const char* text)
{
  uint8_t index = 0;
  if (text)
  {
    while (text[index] != '\0' && index < maxLen)
    {
      dest[index] = text[index];
      ++index;
    }
  }
  dest[index] = '\0';
}

void appendText(char* dest, uint8_t maxLen, const char* suffix)
{
  uint8_t length = findLength(dest, maxLen);
  if (!suffix)
  {
    dest[length] = '\0';
    return;
  }

  uint8_t index = 0;
  while (suffix[index] != '\0' && length < maxLen)
  {
    dest[length++] = suffix[index++];
  }
  dest[length] = '\0';
}

void appendNumber(char* dest, uint8_t maxLen, uint32_t value)
{
  uint8_t length = findLength(dest, maxLen);

  char digits[10];
  uint8_t digitCount = 0;
  do
  {
    digits[digitCount++] = static_cast<char>('0' + (value % 10U));
    value /= 10U;
  } while (value > 0 && digitCount < sizeof(digits));

  while (digitCount > 0 && length < maxLen)
  {
    dest[length++] = digits[--digitCount];
  }
  dest[length] = '\0';
}

#ifdef ASAP_EMBEDDED
// HAL-backed GPIO/delay callback used by U8g2 software SPI on embedded.
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t*,
                                  uint8_t msg,
                                  uint8_t argInt,
                                  void*)
{
  switch (msg)
  {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      return 1;
    case U8X8_MSG_DELAY_MILLI:
      HAL_Delay(argInt);
      return 1;
    case U8X8_MSG_GPIO_DC:
      HAL_GPIO_WritePin(SSD_DC_GPIO_Port,
                        SSD_DC_Pin,
                        argInt ? GPIO_PIN_SET : GPIO_PIN_RESET);
      return 1;
    case U8X8_MSG_GPIO_RESET:
      HAL_GPIO_WritePin(SSD_RESET_GPIO_Port,
                        SSD_RESET_Pin,
                        argInt ? GPIO_PIN_SET : GPIO_PIN_RESET);
      return 1;
    case U8X8_MSG_GPIO_CS:
      HAL_GPIO_WritePin(SSD_CS_GPIO_Port,
                        SSD_CS_Pin,
                        argInt ? GPIO_PIN_SET : GPIO_PIN_RESET);
      return 1;
    case U8X8_MSG_GPIO_SPI_CLOCK:
      HAL_GPIO_WritePin(SSD_CLK_GPIO_Port,
                        SSD_CLK_Pin,
                        argInt ? GPIO_PIN_SET : GPIO_PIN_RESET);
      return 1;
    case U8X8_MSG_GPIO_SPI_DATA:
      HAL_GPIO_WritePin(SSD_MOSI_GPIO_Port,
                        SSD_MOSI_Pin,
                        argInt ? GPIO_PIN_SET : GPIO_PIN_RESET);
      return 1;
    default:
      return 0;
  }
}
#endif

}  // namespace

// Frame factories (same contracts as the original Arduino path). These
// helpers build abstract DisplayFrame objects which are rendered by
// DisplayRenderer using U8g2; they do not talk to hardware directly.
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
      frame.lineCount < DisplayFrame::kMaxLines)
  {
    DisplayLine& footer = frame.lines[frame.lineCount++];
    copyText(footer.text, DisplayLine::kMaxLineLength, "FW ");
    appendText(footer.text, DisplayLine::kMaxLineLength, versionText);
    footer.font = FontStyle::Body;
    footer.y = 60;
  }

  return frame;
}

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

  DisplayLine& sub = frame.lines[frame.lineCount++];
  copyText(sub.text, DisplayLine::kMaxLineLength, "Uptime ");
  appendNumber(sub.text, DisplayLine::kMaxLineLength, uptimeMs / 1000U);
  appendText(sub.text, DisplayLine::kMaxLineLength, "s");
  sub.font = FontStyle::Body;
  sub.y = 40;

  return frame;
}

DisplayFrame makeStatusFrame(const char* line1, const char* line2)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;

  if (line1 && line1[0] != '\0' && frame.lineCount < DisplayFrame::kMaxLines)
  {
    DisplayLine& l1 = frame.lines[frame.lineCount++];
    copyText(l1.text, DisplayLine::kMaxLineLength, line1);
    l1.font = FontStyle::Body;
    l1.y = 24;
  }

  if (line2 && line2[0] != '\0' && frame.lineCount < DisplayFrame::kMaxLines)
  {
    DisplayLine& l2 = frame.lines[frame.lineCount++];
    copyText(l2.text, DisplayLine::kMaxLineLength, line2);
    l2.font = FontStyle::Body;
    l2.y = 44;
  }

  return frame;
}

DisplayFrame makeJoystickFrame(::asap::input::JoyAction action)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;

  DisplayLine& line = frame.lines[frame.lineCount++];
  switch (action)
  {
    case ::asap::input::JoyAction::Left:
      copyText(line.text, DisplayLine::kMaxLineLength, "Left");
      break;
    case ::asap::input::JoyAction::Right:
      copyText(line.text, DisplayLine::kMaxLineLength, "Right");
      break;
    case ::asap::input::JoyAction::Up:
      copyText(line.text, DisplayLine::kMaxLineLength, "Up");
      break;
    case ::asap::input::JoyAction::Down:
      copyText(line.text, DisplayLine::kMaxLineLength, "Down");
      break;
    case ::asap::input::JoyAction::Click:
      copyText(line.text, DisplayLine::kMaxLineLength, "Click");
      break;
    case ::asap::input::JoyAction::Neutral:
    default:
      copyText(line.text, DisplayLine::kMaxLineLength, "Neutral");
      break;
  }

  line.font = FontStyle::Body;
  line.y = 32;

  return frame;
}

DisplayFrame makeMenuRootFrame(uint8_t selectedIndex)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;

  const char* items[] = {"ANOMALY", "TRACKING", "PLAYER DATA", "CONFIG"};
  const uint8_t itemCount =
      static_cast<uint8_t>(sizeof(items) / sizeof(items[0]));

  if (selectedIndex >= itemCount)
  {
    selectedIndex = 0;
  }

  uint8_t visibleStart = 0;
  if (selectedIndex >= 2 && itemCount > 2)
  {
    visibleStart = static_cast<uint8_t>(selectedIndex - 1);
  }

  uint8_t visibleCount = static_cast<uint8_t>(itemCount - visibleStart);
  if (visibleCount > DisplayFrame::kMaxLines)
  {
    visibleCount = DisplayFrame::kMaxLines;
  }

  const uint16_t baseY = 20;
  const uint16_t lineStep = 16;

  for (uint8_t i = 0; i < visibleCount; ++i)
  {
    const uint8_t itemIndex = static_cast<uint8_t>(visibleStart + i);
    DisplayLine& line = frame.lines[frame.lineCount++];
    copyText(line.text, DisplayLine::kMaxLineLength, items[itemIndex]);
    line.font = FontStyle::Body;
    line.y = static_cast<uint16_t>(baseY + i * lineStep);

    if (itemIndex == selectedIndex && line.y >= 10)
    {
      char mark[DisplayLine::kMaxLineLength + 1];
      copyText(mark, DisplayLine::kMaxLineLength, "> ");
      appendText(mark, DisplayLine::kMaxLineLength, line.text);
      copyText(line.text, DisplayLine::kMaxLineLength, mark);
    }
  }

  return frame;
}

DisplayFrame makeMenuTrackingFrame(uint8_t trackingId)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;

  DisplayLine& line = frame.lines[frame.lineCount++];
  copyText(line.text, DisplayLine::kMaxLineLength, "TRACK ");
  appendNumber(line.text, DisplayLine::kMaxLineLength,
               static_cast<uint32_t>(trackingId));
  line.font = FontStyle::Body;
  line.y = 32;

  return frame;
}

DisplayFrame makeMenuAnomalyFrame()
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;

  DisplayLine& line = frame.lines[frame.lineCount++];
  copyText(line.text, DisplayLine::kMaxLineLength, "ANOMALY");
  line.font = FontStyle::Body;
  line.y = 32;

  return frame;
}

DisplayFrame makeAnomalyMainFrame(uint8_t percent, bool showMenuTag)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = showMenuTag;

  frame.progressBarEnabled = true;
  frame.progressX = 8;
  frame.progressY = 48;
  frame.progressWidth = 240;
  frame.progressHeight = 12;
  frame.progressPercent = percent;

  return frame;
}

DisplayFrame makeTrackingMainFrame(uint8_t trackingId,
                                   int16_t rssiAvgDbm,
                                   bool showMenuTag)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = showMenuTag;

  DisplayLine& head = frame.lines[frame.lineCount++];
  copyText(head.text, DisplayLine::kMaxLineLength, "TRACK ");
  appendNumber(head.text, DisplayLine::kMaxLineLength,
               static_cast<uint32_t>(trackingId));
  head.font = FontStyle::Body;
  head.y = 24;

  DisplayLine& rssi = frame.lines[frame.lineCount++];
  copyText(rssi.text, DisplayLine::kMaxLineLength, "RSSI ");

  if (rssiAvgDbm > 0)
  {
    rssiAvgDbm = 0;
  }
  appendNumber(rssi.text, DisplayLine::kMaxLineLength,
               static_cast<uint32_t>(-rssiAvgDbm));
  appendText(rssi.text, DisplayLine::kMaxLineLength, "dBm");
  rssi.font = FontStyle::Body;
  rssi.y = 44;

  return frame;
}

#ifdef ASAP_EMBEDDED
DetectorDisplay::DetectorDisplay(const DisplayPins& pins)
    : pins_(pins),
      u8g2_(),
      initialized_(false),
      lastKind_(FrameKind::None),
      beginCalls_(0u)
{
  (void)pins_;
  lastFrame_.lineCount = 0;
  lastFrame_.spinnerActive = false;
  lastFrame_.spinnerIndex = 0;
  lastFrame_.showMenuTag = false;
  lastFrame_.progressBarEnabled = false;
  lastFrame_.progressX = 0;
  lastFrame_.progressY = 0;
  lastFrame_.progressWidth = 0;
  lastFrame_.progressHeight = 0;
  lastFrame_.progressPercent = 0;
}

bool DetectorDisplay::begin()
{
  ++beginCalls_;
  if (initialized_)
  {
    return true;
  }

  u8g2_Setup_ssd1322_nhd_256x64_f(u8g2_.getU8g2(),
                                  U8G2_R0,
                                  u8x8_byte_4wire_sw_spi,
                                  u8x8_gpio_and_delay_stm32);
  u8g2_.initDisplay();
  u8g2_.setPowerSave(0);
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
  drawAnomalyIndicatorsU8g2(u8g2_,
                            radPercent,
                            thermPercent,
                            chemPercent,
                            psyPercent,
                            radStage,
                            thermStage,
                            chemStage,
                            psyStage);
  u8g2_.sendBuffer();
}

void DetectorDisplay::renderFrame(const DisplayFrame& frame)
{
  lastFrame_ = frame;

  u8g2_.clearBuffer();
  renderFrameU8g2(u8g2_, frame);
  u8g2_.sendBuffer();
}

void DetectorDisplay::drawSpinner(uint8_t, uint16_t, uint16_t)
{
}

void DetectorDisplay::drawCentered(const char*, uint16_t)
{
}

void DetectorDisplay::drawMenuTag()
{
}

void DetectorDisplay::drawProgressBar(const DisplayFrame&)
{
}
#endif  // ASAP_EMBEDDED

}  // namespace asap::display
