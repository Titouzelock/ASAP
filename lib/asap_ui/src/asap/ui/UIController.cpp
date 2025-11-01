#include "asap/ui/UIController.h"
#include <cstring>

namespace asap::ui {

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
      pressStartMs_(0) {}

// Set current anomaly bar fill percentage (0..100) used by the Anomaly main
// page. Values are clamped to the valid range.
void UIController::setAnomalyStrength(uint8_t percent) {
  if (percent > 100) percent = 100;
  anomalyStrength_ = percent;
}

// Feed a new RSSI sample (in dBm) into the tracking EMA. Uses alpha=0.25 for a
// responsive yet stable display.
void UIController::feedTrackingRssi(int16_t rssiDbm) {
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
void UIController::onTick(uint32_t nowMs, const InputSample& sample) {
  // Long-press handling (always allowed)
  if (sample.centerDown && !centerPrev_) {
    pressStartMs_ = nowMs;
  }
  if (sample.centerDown && (nowMs - pressStartMs_ >= kLongPressMs)) {
    state_ = State::MenuRoot;
    selectedIndex_ = 0;
    firstActionDone_ = true;  // first action performed
  }
  centerPrev_ = sample.centerDown;

  // Before first long-press, ignore all non-long-press actions
  if (!firstActionDone_) {
    render();
    return;
  }

  // Handle menu navigation when in menu states
  switch (state_) {
    case State::MenuRoot:
      handleMenuRoot(sample.action);
      break;
    case State::MenuAnomaly:
      handleMenuAnomaly(sample.action);
      break;
    case State::MenuTracking:
      handleMenuTracking(sample.action);
      break;
    case State::MenuConfig:
      handleMenuConfig(sample.action);
      break;
    case State::MainAnomaly:
    case State::MainTracking:
    default:
      // No-op for main pages (inputs ignored until menu is entered)
      break;
  }

  render();
}

// Root menu: Up/Down move selection, Right/Click enter.
void UIController::handleMenuRoot(asap::input::JoyAction action) {
  if (action == asap::input::JoyAction::Up) {
    selectedIndex_ = (selectedIndex_ + 2) % 3;  // wrap 0..2
  } else if (action == asap::input::JoyAction::Down) {
    selectedIndex_ = (selectedIndex_ + 1) % 3;
  } else if (action == asap::input::JoyAction::Right ||
             action == asap::input::JoyAction::Click) {
    if (selectedIndex_ == 0) {
      state_ = State::MenuAnomaly;
    } else if (selectedIndex_ == 1) {
      state_ = State::MenuTracking;
    } else {
      state_ = State::MenuConfig;
    }
  }
}

// Anomaly menu page: Left=back, Right/Click=confirm & exit to main.
void UIController::handleMenuAnomaly(asap::input::JoyAction action) {
  if (action == asap::input::JoyAction::Left) {
    state_ = State::MenuRoot;
  } else if (action == asap::input::JoyAction::Right ||
             action == asap::input::JoyAction::Click) {
    state_ = State::MainAnomaly;  // set as main display mode and exit
  }
}

// Tracking menu page: Up=back, Left/Right adjust ID, Click=confirm & exit.
void UIController::handleMenuTracking(asap::input::JoyAction action) {
  if (action == asap::input::JoyAction::Up) {
    state_ = State::MenuRoot;  // go back to root
  } else if (action == asap::input::JoyAction::Click) {
    state_ = State::MainTracking;  // confirm selection and exit
  } else if (action == asap::input::JoyAction::Left) {
    trackingId_ = static_cast<uint8_t>((trackingId_ + 255) % 256);
  } else if (action == asap::input::JoyAction::Right) {
    trackingId_ = static_cast<uint8_t>((trackingId_ + 1) % 256);
  }
}

// Config menu page: Left=back (placeholder scaffold for future items)
void UIController::handleMenuConfig(asap::input::JoyAction action) {
  if (action == asap::input::JoyAction::Left) {
    state_ = State::MenuRoot;
  }
}

// Render the current UI state using the display frame factories and the
// hardware/native renderer underneath.
void UIController::render() {
  using asap::display::FrameKind;
  using asap::display::DisplayFrame;

  switch (state_) {
    case State::MenuRoot: {
      DisplayFrame f = asap::display::makeMenuRootFrame(selectedIndex_);
      display_.renderCustom(f, FrameKind::Menu);
      break;
    }
    case State::MenuAnomaly: {
      DisplayFrame f = asap::display::makeMenuAnomalyFrame();
      display_.renderCustom(f, FrameKind::Menu);
      break;
    }
    case State::MenuTracking: {
      DisplayFrame f = asap::display::makeMenuTrackingFrame(trackingId_);
      display_.renderCustom(f, FrameKind::Menu);
      break;
    }
    case State::MenuConfig: {
      // Minimal stub page (scaffold)
      DisplayFrame f{};
      f.showMenuTag = true;
      f.lineCount = 0;
      f.spinnerActive = false;
      f.spinnerIndex = 0;
      auto& l1 = f.lines[f.lineCount++];
      std::strncpy(l1.text, "CONFIG", asap::display::DisplayLine::kMaxLineLength);
      l1.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
      l1.font = asap::display::FontStyle::Body;
      l1.y = 28;
      auto& l2 = f.lines[f.lineCount++];
      std::strncpy(l2.text, "COMING SOON", asap::display::DisplayLine::kMaxLineLength);
      l2.text[asap::display::DisplayLine::kMaxLineLength] = '\0';
      l2.font = asap::display::FontStyle::Body;
      l2.y = 48;
      display_.renderCustom(f, FrameKind::Menu);
      break;
    }
    case State::MainAnomaly: {
      DisplayFrame f = asap::display::makeAnomalyMainFrame(anomalyStrength_, false);
      display_.renderCustom(f, FrameKind::MainAnomaly);
      break;
    }
    case State::MainTracking: {
      const int16_t rssi = rssiInit_ ? rssiAvg_ : -100;
      DisplayFrame f = asap::display::makeTrackingMainFrame(trackingId_, rssi, false);
      display_.renderCustom(f, FrameKind::MainTracking);
      break;
    }
    default:
      break;
  }
}

}  // namespace asap::ui
