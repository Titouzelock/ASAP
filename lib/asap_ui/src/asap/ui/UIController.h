#pragma once

#include <stdint.h>

// UIController coordinates input handling (long-press gating + joystick
// navigation) with the display frame factories. It renders logical pages like
// Menu, Anomaly main HUD, and Tracking main HUD by delegating to
// asap::display frame builders. This isolates UI state and timing from
// rendering details.

#include <asap/display/DetectorDisplay.h>
#include <asap/input/Joystick.h>

namespace asap::ui {

// High-level UI states for detector.
// High-level UI states for detector. Menu states show the "MENU" tag; main
// states are the operational HUDs selected from the menu.
enum class State : uint8_t {
  MainAnomaly,
  MainTracking,
  MenuRoot,
  MenuAnomaly,
  MenuTracking,
  MenuConfig,
};

// Debounced input snapshot passed to the controller on each tick.
// - centerDown: level signal for long-press detection
// - action: edge-triggered high-level joystick action for navigation
struct InputSample {
  bool centerDown;                    // true while the center button is held
  asap::input::JoyAction action;      // debounced directional/click edge
};

class UIController {
 public:
  explicit UIController(asap::display::DetectorDisplay& display);

  // Advance UI state machine and render.
  // Call at a fixed cadence with current time and input sample.
  void onTick(uint32_t nowMs, const InputSample& sample);

  // External signal hooks
  void setAnomalyStrength(uint8_t percent);  // 0..100 (bar fill)
  void feedTrackingRssi(int16_t rssiDbm);    // EMA input (dBm)

  // For testing/inspection
  State state() const { return state_; }
  uint8_t trackingId() const { return trackingId_; }

 private:
  void render();
  // Menu handlers apply the agreed navigation mapping
  void handleMenuRoot(asap::input::JoyAction action);
  void handleMenuAnomaly(asap::input::JoyAction action);
  void handleMenuTracking(asap::input::JoyAction action);
  void handleMenuConfig(asap::input::JoyAction action);

  asap::display::DetectorDisplay& display_;
  State state_;
  uint8_t selectedIndex_;  // root menu selection 0..2
  uint8_t trackingId_;
  int16_t rssiAvg_;        // EMA accumulator (in dBm)
  bool rssiInit_;
  uint8_t anomalyStrength_;

  // Long press gating
  bool firstActionDone_;   // becomes true after initial long-press
  bool centerPrev_;        // last sampled level for center button
  uint32_t pressStartMs_;  // timestamp when center transitioned to down
  static constexpr uint32_t kLongPressMs = 1000;  // confirmed requirement
};

}  // namespace asap::ui
