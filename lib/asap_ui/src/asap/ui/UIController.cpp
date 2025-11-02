#include "asap/ui/UIController.h"
#include <cstring>

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
      firstActionDone_(false),
      centerPrev_(false),
      pressStartMs_(0)
{
}

// Set current anomaly bar fill percentage (0..100) used by the Anomaly main
// page. Values are clamped to the valid range.
void UIController::setAnomalyStrength(uint8_t percent)
{
  if (percent > 100) percent = 100;
  anomalyStrength_ = percent;
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

void UIController::RenderMainAnomaly(UIController& self)
{
  using asap::display::FrameKind;
  asap::display::DisplayFrame f = asap::display::makeAnomalyMainFrame(self.anomalyStrength_, false);
  self.display_.renderCustom(f, FrameKind::MainAnomaly);
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
