#include "asap/display/DisplayTypes.h"

#include <stddef.h>

namespace asap::display
{

namespace
{

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

}  // namespace

DisplayFrame makeBootFrame(const char* versionText)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;
  frame.progressBarEnabled = false;

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
  frame.showMenuTag = false;
  frame.progressBarEnabled = false;

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

DisplayFrame makeStatusFrame(const char* line1, const char* line2)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;
  frame.progressBarEnabled = false;

  if (line1 && line1[0] != '\0')
  {
    DisplayLine& l1 = frame.lines[frame.lineCount++];
    copyText(l1.text, DisplayLine::kMaxLineLength, line1);
    l1.font = FontStyle::Body;
    l1.y = 28;
  }

  if (line2 && line2[0] != '\0' && frame.lineCount < DisplayFrame::kMaxLines)
  {
    DisplayLine& l2 = frame.lines[frame.lineCount++];
    copyText(l2.text, DisplayLine::kMaxLineLength, line2);
    l2.font = FontStyle::Body;
    l2.y = 48;
  }

  return frame;
}

DisplayFrame makeJoystickFrame(::asap::input::JoyAction action)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;
  frame.progressBarEnabled = false;

  DisplayLine& headline = frame.lines[frame.lineCount++];
  copyText(headline.text, DisplayLine::kMaxLineLength, "Joystick");
  headline.font = FontStyle::Body;
  headline.y = 24;

  static constexpr const char* kActionNames[] = {
      "NEUTRAL", "LEFT", "RIGHT", "UP", "DOWN", "CLICK", "LONGPRESS"};
  uint8_t idx = static_cast<uint8_t>(action);
  if (idx >= sizeof(kActionNames) / sizeof(kActionNames[0]))
  {
    idx = 0;
  }

  DisplayLine& actionLine = frame.lines[frame.lineCount++];
  copyText(actionLine.text, DisplayLine::kMaxLineLength, kActionNames[idx]);
  actionLine.font = FontStyle::Body;
  actionLine.y = 48;

  return frame;
}

DisplayFrame makeMenuRootFrame(uint8_t selectedIndex)
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;
  frame.progressBarEnabled = false;

  static constexpr const char* kItems[] = {"ANOMALY", "TRACKING", "CONFIG"};
  const uint8_t count = static_cast<uint8_t>(sizeof(kItems) / sizeof(kItems[0]));

  for (uint8_t i = 0; i < count && frame.lineCount < DisplayFrame::kMaxLines; ++i)
  {
    DisplayLine& line = frame.lines[frame.lineCount++];
    if (i == selectedIndex)
    {
      copyText(line.text, DisplayLine::kMaxLineLength, "> ");
      appendText(line.text, DisplayLine::kMaxLineLength, kItems[i]);
    }
    else
    {
      copyText(line.text, DisplayLine::kMaxLineLength, "  ");
      appendText(line.text, DisplayLine::kMaxLineLength, kItems[i]);
    }
    line.font = FontStyle::Body;
    line.y = static_cast<uint16_t>(20 + i * 18);
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
  frame.progressBarEnabled = false;

  DisplayLine& header = frame.lines[frame.lineCount++];
  copyText(header.text, DisplayLine::kMaxLineLength, "TRACK ID");
  header.font = FontStyle::Body;
  header.y = 24;

  DisplayLine& value = frame.lines[frame.lineCount++];
  copyText(value.text, DisplayLine::kMaxLineLength, "#");
  appendNumber(value.text, DisplayLine::kMaxLineLength, trackingId);
  value.font = FontStyle::Body;
  value.y = 48;

  return frame;
}

DisplayFrame makeMenuAnomalyFrame()
{
  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = false;
  frame.progressBarEnabled = false;

  static constexpr const char* kLines[] = {
      "Set Main Anomaly",
      "Left to exit"};

  for (uint8_t i = 0; i < sizeof(kLines) / sizeof(kLines[0]) &&
                      frame.lineCount < DisplayFrame::kMaxLines;
       ++i)
  {
    DisplayLine& line = frame.lines[frame.lineCount++];
    copyText(line.text, DisplayLine::kMaxLineLength, kLines[i]);
    line.font = FontStyle::Body;
    line.y = static_cast<uint16_t>(24 + i * 18);
  }

  return frame;
}

DisplayFrame makeAnomalyMainFrame(uint8_t percent, bool showMenuTag)
{
  if (percent > 100)
  {
    percent = 100;
  }

  DisplayFrame frame{};
  frame.lineCount = 0;
  frame.spinnerActive = false;
  frame.spinnerIndex = 0;
  frame.showMenuTag = showMenuTag;
  frame.progressBarEnabled = true;
  frame.progressX = 16;
  frame.progressY = 46;
  frame.progressWidth = 224;
  frame.progressHeight = 12;
  frame.progressPercent = percent;

  DisplayLine& title = frame.lines[frame.lineCount++];
  copyText(title.text, DisplayLine::kMaxLineLength, "ANOMALY");
  title.font = FontStyle::Body;
  title.y = 26;

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
  frame.progressBarEnabled = false;

  DisplayLine& title = frame.lines[frame.lineCount++];
  copyText(title.text, DisplayLine::kMaxLineLength, "TRACK ");
  appendNumber(title.text, DisplayLine::kMaxLineLength, trackingId);
  title.font = FontStyle::Body;
  title.y = 20;

  DisplayLine& line2 = frame.lines[frame.lineCount++];
  copyText(line2.text, DisplayLine::kMaxLineLength, "RSSI ");
  if (rssiAvgDbm < 0)
  {
    appendText(line2.text, DisplayLine::kMaxLineLength, "-");
    appendNumber(line2.text, DisplayLine::kMaxLineLength,
                 static_cast<uint32_t>(-rssiAvgDbm));
  }
  else
  {
    appendNumber(line2.text, DisplayLine::kMaxLineLength,
                 static_cast<uint32_t>(rssiAvgDbm));
  }
  appendText(line2.text, DisplayLine::kMaxLineLength, "dBm");
  line2.font = FontStyle::Body;
  line2.y = 52;

  return frame;
}

}  // namespace asap::display
