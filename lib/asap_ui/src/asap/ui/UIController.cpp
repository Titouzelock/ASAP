#include "asap/ui/UIController.h"
#include <cstring>
// Player data access for the Player Data page
#include <asap/player/PlayerData.h>
#include <asap/player/Storage.h>
#include <cstdio>

namespace asap::ui
{

// Construct a UI controller bound to a display instance. Starts in the default
// main page (Anomaly) with first-action gating enabled (requires a 1000ms
// long-press on the center button to enter the menu the first time).
UIController::UIController(asap::display::DetectorDisplay& display)
    : display_(display),
      state_(State::MainAnomaly),
      selectedIndex_(0),
      trackingId_(0),
      rssiAvg_(-100),
      rssiInit_(false),
      anomalyStrength_(0),
      anomalyRad_(0), anomalyTherm_(0), anomalyChem_(0), anomalyPsy_(0),
      stageRad_(0), stageTherm_(0), stageChem_(0), stagePsy_(0),
      firstActionDone_(false),
      centerPrev_(false),
      pressStartMs_(0)
{
}

// Legacy: set current anomaly bar fill percentage (0..100). Kept for
// compatibility; the new HUD uses per-channel exposure+stage instead.
void UIController::setAnomalyStrength(uint8_t percent)
{
  if (percent > 100) percent = 100;
  anomalyStrength_ = percent;
}

// Set per-channel arc progress for anomaly indicators (0..100% of current turn)
void UIController::setAnomalyExposure(uint8_t rad, uint8_t therm, uint8_t chem, uint8_t psy)
{
  anomalyRad_ = (rad > 100) ? 100 : rad;
  anomalyTherm_ = (therm > 100) ? 100 : therm;
  anomalyChem_ = (chem > 100) ? 100 : chem;
  anomalyPsy_ = (psy > 100) ? 100 : psy;
}

// Set per-channel stage (0 none/-, 1 I, 2 II, 3 III)
void UIController::setAnomalyStage(uint8_t rad, uint8_t therm, uint8_t chem, uint8_t psy)
{
  stageRad_ = (rad > 3) ? 3 : rad;
  stageTherm_ = (therm > 3) ? 3 : therm;
  stageChem_ = (chem > 3) ? 3 : chem;
  stagePsy_ = (psy > 3) ? 3 : psy;
}

// Feed a new RSSI sample (in dBm) into the tracking EMA. Uses alpha=0.25 for a
// responsive yet stable display.
void UIController::feedTrackingRssi(int16_t rssiDbm)
{
  // EMA with alpha ~0.25
  if (!rssiInit_) {
    rssiAvg_ = rssiDbm;
    rssiInit_ = true;
    return;
  }
  const int32_t alphaNum = 1;   // 1/4
  const int32_t alphaDen = 4;
  rssiAvg_ = static_cast<int16_t>((alphaNum * static_cast<int32_t>(rssiDbm) +
                                   (alphaDen - alphaNum) * static_cast<int32_t>(rssiAvg_)) /
                                  alphaDen);
}

// Advance the UI state machine and render the resulting page. Applies first
// action gating so that the very first user interaction must be a long-press
// on the center button (>=1000ms) to enter the menu. Subsequent long-presses
// also open the menu, as expected.
void UIController::onTick(uint32_t nowMs, const InputSample& sample)
{
  // Long-press handling (always allowed)
  if (sample.centerDown && !centerPrev_)
  {
    pressStartMs_ = nowMs;
  }
  if (sample.centerDown && (nowMs - pressStartMs_ >= kLongPressMs))
  {
    state_ = State::MenuRoot;
    selectedIndex_ = 0;
    firstActionDone_ = true;  // first action performed
  }
  centerPrev_ = sample.centerDown;

  // Before first long-press, ignore all non-long-press actions
  if (!firstActionDone_)
  {
    render();
    return;
  }

  // Apply joystick inversion preferences before navigation.
  // The physical device may be mounted differently between builds; invert
  // mapping here so the rest of the state machine can stay agnostic.
  asap::input::JoyAction act = sample.action;
  if (invertX_)
  {
    if (act == asap::input::JoyAction::Left) act = asap::input::JoyAction::Right;
    else if (act == asap::input::JoyAction::Right) act = asap::input::JoyAction::Left;
  }
  if (invertY_)
  {
    if (act == asap::input::JoyAction::Up) act = asap::input::JoyAction::Down;
    else if (act == asap::input::JoyAction::Down) act = asap::input::JoyAction::Up;
  }

  // Handle navigation using declarative graph with transformed action.
  navigate(act);

  render();
}

// Render the current UI state using the display frame factories and the
// hardware/native renderer underneath.
void UIController::render()
{
  using asap::display::FrameKind;
  using asap::display::DisplayFrame;

  // Use the declarative render hook
  const PageNode* page = findPage(state_);
  if (page && page->render)
  {
    page->render(*this);
  }
}

// Navigation using declarative graph. Order of operations:
//  1) Give the page a chance to mutate selection or local state (onAction)
//  2) If the action is the page's backAction, switch to the parent
//  3) If the action matches confirmMask, either enter the selected child or
//     jump to the configured confirmTarget
void UIController::navigate(asap::input::JoyAction action)
{
  const PageNode* page = findPage(state_);
  if (!page)
  {
    return;
  }

  // Per-page action hook (selection changes, ID tweaks, etc.)
  if (page->onAction)
  {
    page->onAction(*this, action);
  }

  // Back navigation
  if (page->backAction != asap::input::JoyAction::Neutral && action == page->backAction)
  {
    state_ = page->parent;
    // Clamp selection for the new page
    const PageNode* np = findPage(state_);
    const uint8_t ncount = (np && np->childCount > 0) ? np->childCount : 0;
    if (ncount == 0)
    {
      selectedIndex_ = 0;
    }
    else if (selectedIndex_ >= ncount)
    {
      selectedIndex_ = 0;
    }
    return;
  }

  // Confirmation / Enter
  const uint8_t bit = ActionBit(action);
  if (bit != 0 && (page->confirmMask & bit) != 0)
  {
    if (page->confirm == ConfirmBehavior::EnterSelectedChild && page->children && page->childCount > 0)
    {
      const uint8_t idx = (selectedIndex_ < page->childCount) ? selectedIndex_ : 0;
      state_ = page->children[idx];
      // When entering a new menu/list, start at the first item
      const PageNode* np = findPage(state_);
      if (np && np->childCount > 0)
      {
        selectedIndex_ = 0;
      }
      // Reset Player Data scroll when entering that page
      if (state_ == State::MenuPlayerData)
      {
        playerDataOffset_ = 0;
      }
      return;
    }
    if (page->confirm == ConfirmBehavior::GoToTarget)
    {
      state_ = page->confirmTarget;
      const PageNode* np = findPage(state_);
      const uint8_t ncount = (np && np->childCount > 0) ? np->childCount : 0;
      if (ncount == 0)
      {
        selectedIndex_ = 0;
      }
      return;
    }
  }
}

  // Page lookup against the static definition in the header
const UIController::PageNode* UIController::findPage(State id)
{
  for (const auto& n : kPages)
  {
    if (n.id == id)
    {
      return &n;
    }
  }
  return nullptr;
}

// Render hooks
void UIController::RenderMenuRoot(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f = asap::display::makeMenuRootFrame(self.selectedIndex_);
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::RenderMenuAnomaly(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f = asap::display::makeMenuAnomalyFrame();
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::RenderMenuTracking(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f = asap::display::makeMenuTrackingFrame(self.trackingId_);
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::RenderMenuConfig(UIController& self)
{
  using asap::display::FrameKind;
  // Now replaced by RenderMenuConfigList (kept for compatibility if referenced)
  RenderMenuConfigList(self);
}

// --- Player Data page helpers (two‑phase description wrapping) ---
namespace
{
  using asap::display::DisplayLine;

  uint16_t countDescriptionSegments(const char* desc)
  {
    if (!desc || desc[0] == '\0') return 0;
    const uint8_t firstWidth = DisplayLine::kMaxLineLength;
    const uint8_t contWidth  = (DisplayLine::kMaxLineLength > 2) ? static_cast<uint8_t>(DisplayLine::kMaxLineLength - 2) : 0;

    uint16_t total = 0;
    size_t i = 0;
    while (i < asap::player::kDescriptionSize)
    {
      // Phase 1: one logical line (terminated by NUL/CR/LF)
      const size_t lineStart = i;
      while (i < asap::player::kDescriptionSize)
      {
        char c = desc[i];
        if (c == '\0' || c == '\r' || c == '\n') break;
        ++i;
      }
      const size_t lineLen = i - lineStart;

      // Phase 2: segments for this logical line
      if (lineLen == 0)
      {
        ++total; // empty line -> empty segment
      }
      else if (lineLen <= firstWidth)
      {
        ++total;
      }
      else
      {
        size_t remaining = lineLen - firstWidth;
        uint16_t contSegs = (contWidth == 0) ? static_cast<uint16_t>(remaining)
                                             : static_cast<uint16_t>((remaining + contWidth - 1) / contWidth);
        total = static_cast<uint16_t>(total + 1 + contSegs);
      }

      // consume exactly one break (CR or LF); CRLF yields an empty line next loop
      if (i >= asap::player::kDescriptionSize) break;
      char br = desc[i];
      if (br == '\0') break;
      ++i;
    }
    return total;
  }

  bool renderDescriptionSegment(uint16_t target, const char* desc, char* out)
  {
    const uint8_t kMax = DisplayLine::kMaxLineLength;
    const uint8_t firstWidth = kMax;
    const uint8_t contWidth  = (kMax > 2) ? static_cast<uint8_t>(kMax - 2) : 0;

    if (!desc) { out[0] = '\0'; return false; }
    size_t i = 0;
    uint16_t segIndex = 0;
    while (i < asap::player::kDescriptionSize)
    {
      // Phase 1: logical line
      const size_t lineStart = i;
      while (i < asap::player::kDescriptionSize)
      {
        char c = desc[i];
        if (c == '\0' || c == '\r' || c == '\n') break;
        ++i;
      }
      const size_t lineLen = i - lineStart;

      // Phase 2: emit segment if matches target
      if (lineLen == 0)
      {
        if (segIndex == target) { out[0] = '\0'; return true; }
        ++segIndex;
      }
      else if (lineLen <= firstWidth)
      {
        if (segIndex == target)
        {
          const size_t copyLen = (lineLen < kMax) ? lineLen : kMax;
          for (size_t j = 0; j < copyLen; ++j) out[j] = desc[lineStart + j];
          out[copyLen] = '\0';
          return true;
        }
        ++segIndex;
      }
      else
      {
        // first segment
        if (segIndex == target)
        {
          const size_t copyLen = firstWidth;
          for (size_t j = 0; j < copyLen; ++j) out[j] = desc[lineStart + j];
          out[copyLen] = '\0';
          return true;
        }
        ++segIndex;
        // continuations
        size_t remaining = lineLen - firstWidth;
        size_t offset = firstWidth;
        while (remaining > 0)
        {
          const size_t chunk = (remaining < contWidth) ? remaining : contWidth;
          if (segIndex == target)
          {
            if (kMax >= 2)
            {
              out[0] = '>';
              out[1] = ' ';
              for (size_t j = 0; j < chunk && (2 + j) < kMax; ++j)
              {
                out[2 + j] = desc[lineStart + offset + j];
              }
              out[2 + chunk] = '\0';
            }
            else { out[0] = '\0'; }
            return true;
          }
          ++segIndex;
          offset += chunk;
          remaining -= chunk;
        }
      }

      // consume a single break
      if (i >= asap::player::kDescriptionSize) break;
      char br = desc[i]; if (br == '\0') break; ++i;
    }
    out[0] = '\0';
    return false;
  }

  // Build visual content for a given index
  void formatPlayerDataLine(uint16_t index, const asap::player::PlayerPersistent& p, char* out)
  {
    using namespace asap::display;
    if (index == 0)
    {
      std::strncpy(out, "PLAYER DATA", DisplayLine::kMaxLineLength);
      out[DisplayLine::kMaxLineLength] = '\0';
      return;
    }
    if (index == 1)
    {
      std::strncpy(out, "Version ", DisplayLine::kMaxLineLength);
      out[DisplayLine::kMaxLineLength] = '\0';
      uint8_t v = p.version; char digits[3]; uint8_t d=0;
      do { digits[d++] = static_cast<char>('0' + (v % 10U)); v/=10U; } while (v>0 && d<sizeof(digits));
      while (d>0 && std::strlen(out) < DisplayLine::kMaxLineLength) { size_t L=std::strlen(out); out[L]=digits[--d]; out[L+1]='\0'; }
      return;
    }
    auto append_u8 = [&](const char* label, uint8_t val)
    {
      size_t L = std::strlen(out);
      const char* s = label; while (*s && L < DisplayLine::kMaxLineLength) { out[L++] = *s++; }
      out[L] = '\0'; char digits[3]; uint8_t d=0; uint8_t vv=val;
      do { digits[d++] = static_cast<char>('0' + (vv % 10U)); vv/=10U; } while (vv>0 && d<sizeof(digits));
      while (d>0 && L < DisplayLine::kMaxLineLength) { out[L++] = digits[--d]; }
      out[L] = '\0';
    };
    if (index == 2)
    {
      out[0] = '\0'; append_u8("FIRE ", p.logic.fire_resistance);
      if (std::strlen(out) + 3 < DisplayLine::kMaxLineLength) std::strncat(out, " | ", DisplayLine::kMaxLineLength - std::strlen(out));
      append_u8("PSY ", p.logic.psy_resistance); return;
    }
    if (index == 3)
    {
      out[0] = '\0'; append_u8("RAD ", p.logic.radiation_resistance);
      if (std::strlen(out) + 3 < DisplayLine::kMaxLineLength) std::strncat(out, " | ", DisplayLine::kMaxLineLength - std::strlen(out));
      append_u8("CHEM ", p.logic.chemical_resistance); return;
    }
    if (index == 4)
    {
      out[0] = '\0'; append_u8("ARM ", p.logic.armor);
      if (std::strlen(out) + 3 < DisplayLine::kMaxLineLength) std::strncat(out, " | ", DisplayLine::kMaxLineLength - std::strlen(out));
      append_u8("FAC ", p.logic.faction); return;
    }
    if (index == 5)
    {
      std::strncpy(out, "MONEY ", DisplayLine::kMaxLineLength);
      out[DisplayLine::kMaxLineLength] = '\0';
      uint32_t rub = static_cast<uint32_t>(p.logic.money_units) * 100U; char digits[10]; uint8_t d=0;
      do { digits[d++] = static_cast<char>('0' + (rub % 10U)); rub/=10U; } while (rub>0 && d<sizeof(digits));
      while (d>0 && std::strlen(out) < DisplayLine::kMaxLineLength) { size_t L=std::strlen(out); out[L]=digits[--d]; out[L+1]='\0'; }
      return;
    }
    const uint16_t descLineBase = 6;
    const uint16_t target = static_cast<uint16_t>(index - descLineBase);
    if (!renderDescriptionSegment(target, p.description, out)) out[0] = '\0';
    return;
  }

  uint16_t totalPlayerDataLines(const asap::player::PlayerPersistent& p)
  {
    return static_cast<uint16_t>(6 + countDescriptionSegments(p.description));
  }
}

void UIController::RenderMenuPlayerData(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f{};
  // Hide MENU tag on Player Data page
  f.showMenuTag = false;
  f.lineCount = 0;
  f.spinnerActive = false;
  f.spinnerIndex = 0;

  // Load current persistent data
  asap::player::PlayerPersistent p{};
  if (!asap::player::loadPersistent(p))
  {
    asap::player::initDefaults(p);
  }

  const uint16_t total = totalPlayerDataLines(p);
  if (self.playerDataOffset_ > total)
  {
    self.playerDataOffset_ = 0;
  }

  const uint16_t ys[3] = {20, 38, 56};
  for (uint8_t i = 0; i < 3; ++i)
  {
    const uint16_t li = static_cast<uint16_t>(self.playerDataOffset_ + i);
    if (li >= total) break;
    auto& line = f.lines[f.lineCount++];
    char buf[asap::display::DisplayLine::kMaxLineLength + 1];
    formatPlayerDataLine(li, p, buf);
    std::strncpy(line.text, buf, asap::display::DisplayLine::kMaxLineLength);
    line.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
    line.font = asap::display::FontStyle::Body;
    line.y = ys[i];
  }

  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::ActionMenuPlayerData(UIController& self, asap::input::JoyAction action)
{
  // Up: scroll back by 3, clamp at 0
  if (action == asap::input::JoyAction::Up)
  {
    if (self.playerDataOffset_ >= 3) self.playerDataOffset_ = static_cast<uint16_t>(self.playerDataOffset_ - 3);
    return;
  }
  if (action == asap::input::JoyAction::Down)
  {
    // Compute total lines to decide if we exit
    asap::player::PlayerPersistent p{};
    if (!asap::player::loadPersistent(p)) { asap::player::initDefaults(p); }
    const uint16_t total = totalPlayerDataLines(p);
    uint16_t nextOffset = static_cast<uint16_t>(self.playerDataOffset_ + 3);
    if (nextOffset >= total)
    {
      self.state_ = State::MenuRoot;
      self.selectedIndex_ = 0;
      self.playerDataOffset_ = 0;
      return;
    }
    self.playerDataOffset_ = nextOffset;
    return;
  }
  if (action == asap::input::JoyAction::Click || action == asap::input::JoyAction::Left)
  {
    self.state_ = State::MenuRoot;
    self.selectedIndex_ = 0;
    self.playerDataOffset_ = 0;
    return;
  }
}

void UIController::RenderMainAnomaly(UIController& self)
{
  using asap::display::FrameKind;
  // New anomaly HUD: four indicators with circular progress and stage labels.
  self.display_.drawAnomalyIndicators(self.anomalyRad_, self.anomalyTherm_,
                                      self.anomalyChem_, self.anomalyPsy_,
                                      self.stageRad_, self.stageTherm_,
                                      self.stageChem_, self.stagePsy_);
}

void UIController::RenderMainTracking(UIController& self)
{
  using asap::display::FrameKind;
  const int16_t rssi = self.rssiInit_ ? self.rssiAvg_ : -100;
  asap::display::DisplayFrame f = asap::display::makeTrackingMainFrame(self.trackingId_, rssi, false);
  self.display_.renderCustom(f, FrameKind::MainTracking);
}

// Action hooks
void UIController::ActionMenuRoot(UIController& self, asap::input::JoyAction action)
{
  const PageNode* page = UIController::findPage(self.state_);
  const uint8_t count = (page && page->childCount > 0) ? page->childCount : 0;
  if (count == 0)
  {
    return;
  }

  // Clamp selection to valid range if needed
  if (self.selectedIndex_ >= count)
  {
    self.selectedIndex_ = 0;
  }

  if (action == asap::input::JoyAction::Up)
  {
    if (self.selectedIndex_ == 0)
    {
      self.selectedIndex_ = static_cast<uint8_t>(count - 1);
    }
    else
    {
      self.selectedIndex_ = static_cast<uint8_t>(self.selectedIndex_ - 1);
    }
  }
  else if (action == asap::input::JoyAction::Down)
  {
    self.selectedIndex_ = static_cast<uint8_t>((self.selectedIndex_ + 1) % count);
  }
}

void UIController::ActionMenuList(UIController& self, asap::input::JoyAction action)
{
  const PageNode* page = UIController::findPage(self.state_);
  const uint8_t count = (page && page->childCount > 0) ? page->childCount : 0;
  if (count == 0)
  {
    return;
  }

  if (self.selectedIndex_ >= count)
  {
    self.selectedIndex_ = 0;
  }

  if (action == asap::input::JoyAction::Up)
  {
    if (self.selectedIndex_ == 0)
    {
      self.selectedIndex_ = static_cast<uint8_t>(count - 1);
    }
    else
    {
      self.selectedIndex_ = static_cast<uint8_t>(self.selectedIndex_ - 1);
    }
  }
  else if (action == asap::input::JoyAction::Down)
  {
    self.selectedIndex_ = static_cast<uint8_t>((self.selectedIndex_ + 1) % count);
  }
}

void UIController::RenderMenuConfigList(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f{};
  f.showMenuTag = true;
  f.lineCount = 0;
  f.spinnerActive = false;
  f.spinnerIndex = 0;

  // Windowed list: previous, selected, next (3 lines max)
  const char* labels[5] = {"INVERT X JOYSTICK", "INVERT Y JOYSTICK", "ROTATE DISPLAY", "RSSI CALIB", "VERSION"};
  const uint16_t ys[3] = {20, 38, 56};

  const PageNode* page = UIController::findPage(self.state_);
  const uint8_t count = (page && page->childCount > 0) ? page->childCount : 0;
  if (count == 0)
  {
    self.display_.renderCustom(f, FrameKind::Menu);
    return;
  }
  if (self.selectedIndex_ >= count) self.selectedIndex_ = 0;

  // Show three neighbor items around the selected index for context
  const uint8_t prev = static_cast<uint8_t>((self.selectedIndex_ + count - 1) % count);
  const uint8_t curr = self.selectedIndex_;
  const uint8_t next = static_cast<uint8_t>((self.selectedIndex_ + 1) % count);

  const uint8_t order[3] = {prev, curr, next};
  for (uint8_t i = 0; i < 3 && i < count; ++i)
  {
    auto& line = f.lines[f.lineCount++];
    if (order[i] == curr)
    {
      std::strncpy(line.text, "> ", asap::display::DisplayLine::kMaxLineLength);
    }
    else
    {
      std::strncpy(line.text, "  ", asap::display::DisplayLine::kMaxLineLength);
    }
    line.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
    std::strncat(line.text, labels[order[i]], asap::display::DisplayLine::kMaxLineLength - std::strlen(line.text));
    line.font = asap::display::FontStyle::Body;
    line.y = ys[i];
  }

  self.display_.renderCustom(f, FrameKind::Menu);
}

static void SetOnOffLine(asap::display::DisplayFrame& f, bool on)
{
  auto& l2 = f.lines[f.lineCount++];
  std::strncpy(l2.text, on ? "ON" : "OFF", asap::display::DisplayLine::kMaxLineLength);
  l2.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l2.font = asap::display::FontStyle::Body;
  l2.y = 48;
}

void UIController::RenderConfigInvertX(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f{};
  f.showMenuTag = true;
  f.lineCount = 0;
  f.spinnerActive = false;
  f.spinnerIndex = 0;
  auto& l1 = f.lines[f.lineCount++];
  std::strncpy(l1.text, "INVERT X JOYSTICK", asap::display::DisplayLine::kMaxLineLength);
  l1.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l1.font = asap::display::FontStyle::Body;
  l1.y = 28;
  SetOnOffLine(f, self.invertX_);
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::RenderConfigInvertY(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f{};
  f.showMenuTag = true;
  f.lineCount = 0;
  f.spinnerActive = false;
  f.spinnerIndex = 0;
  auto& l1 = f.lines[f.lineCount++];
  std::strncpy(l1.text, "INVERT Y JOYSTICK", asap::display::DisplayLine::kMaxLineLength);
  l1.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l1.font = asap::display::FontStyle::Body;
  l1.y = 28;
  SetOnOffLine(f, self.invertY_);
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::RenderConfigRotate(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f{};
  f.showMenuTag = true;
  f.lineCount = 0;
  f.spinnerActive = false;
  f.spinnerIndex = 0;
  auto& l1 = f.lines[f.lineCount++];
  std::strncpy(l1.text, "ROTATE DISPLAY", asap::display::DisplayLine::kMaxLineLength);
  l1.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l1.font = asap::display::FontStyle::Body;
  l1.y = 28;
  SetOnOffLine(f, self.rotateDisplay_);
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::RenderConfigRssi(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f{};
  f.showMenuTag = true;
  f.lineCount = 0;
  f.spinnerActive = false;
  f.spinnerIndex = 0;
  auto& l1 = f.lines[f.lineCount++];
  std::strncpy(l1.text, "RSSI CALIB", asap::display::DisplayLine::kMaxLineLength);
  l1.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l1.font = asap::display::FontStyle::Body;
  l1.y = 28;
  auto& l2 = f.lines[f.lineCount++];
  std::strncpy(l2.text, "COMING SOON", asap::display::DisplayLine::kMaxLineLength);
  l2.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l2.font = asap::display::FontStyle::Body;
  l2.y = 48;
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::RenderConfigVersion(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f{};
  f.showMenuTag = true;
  f.lineCount = 0;
  f.spinnerActive = false;
  f.spinnerIndex = 0;
  auto& l1 = f.lines[f.lineCount++];
  std::strncpy(l1.text, "VERSION", asap::display::DisplayLine::kMaxLineLength);
  l1.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l1.font = asap::display::FontStyle::Body;
  l1.y = 28;
  auto& l2 = f.lines[f.lineCount++];
#ifdef ASAP_VERSION
  std::strncpy(l2.text, "FW " ASAP_VERSION, asap::display::DisplayLine::kMaxLineLength);
#else
  std::strncpy(l2.text, "FW dev", asap::display::DisplayLine::kMaxLineLength);
#endif
  l2.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
  l2.font = asap::display::FontStyle::Body;
  l2.y = 48;
  self.display_.renderCustom(f, FrameKind::Menu);
}

void UIController::ActionConfigToggleX(UIController& self, asap::input::JoyAction action)
{
  if (action == asap::input::JoyAction::Right || action == asap::input::JoyAction::Click)
  {
    self.invertX_ = !self.invertX_;
  }
}

void UIController::ActionConfigToggleY(UIController& self, asap::input::JoyAction action)
{
  if (action == asap::input::JoyAction::Right || action == asap::input::JoyAction::Click)
  {
    self.invertY_ = !self.invertY_;
  }
}

void UIController::ActionConfigToggleRotate(UIController& self, asap::input::JoyAction action)
{
  if (action == asap::input::JoyAction::Right || action == asap::input::JoyAction::Click)
  {
    self.rotateDisplay_ = !self.rotateDisplay_;
    // Propagate to display backends
    self.display_.setRotation180(self.rotateDisplay_);
  }
}

void UIController::ActionMenuTracking(UIController& self, asap::input::JoyAction action)
{
  if (action == asap::input::JoyAction::Left)
  {
    self.trackingId_ = static_cast<uint8_t>((self.trackingId_ + 255) % 256);
  }
  else if (action == asap::input::JoyAction::Right)
  {
    self.trackingId_ = static_cast<uint8_t>((self.trackingId_ + 1) % 256);
  }
}

void UIController::ActionNoop(UIController&, asap::input::JoyAction)
{
}

}  // namespace asap::ui
