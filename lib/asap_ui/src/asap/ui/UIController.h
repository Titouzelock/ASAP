#pragma once

#include <stdint.h>

// UIController coordinates input handling (long-press gating + joystick
// navigation) with the display frame factories. It renders logical pages like
// Menu, Anomaly main HUD, and Tracking main HUD by delegating to
// asap::display frame builders. This isolates UI state and timing from
// rendering details.

#include <asap/display/DisplayDriver.h>
#include <asap/input/Joystick.h>

namespace asap::ui
{

// Utility to compute an action bit from JoyAction (namespace-scope to allow
// use in inline constexpr graph definitions).
inline constexpr uint8_t ActionBit(asap::input::JoyAction a)
{
  return static_cast<uint8_t>(1u << static_cast<uint8_t>(a));
}

// High-level UI states for detector.
// High-level UI states for detector. Menu states show the "MENU" tag; main
// states are the operational HUDs selected from the menu.
// UI page identifiers. Menu states show the "MENU" tag; main states are the
// operational HUDs selected from the menu.
enum class State : uint8_t
{
  MainAnomaly,
  MainTracking,
  MenuRoot,
  MenuAnomaly,
  MenuTracking,
  MenuConfig,
  // Config submenu items
  MenuConfigInvertX,
  MenuConfigInvertY,
  MenuConfigRotate,
  MenuConfigRssiCal,
  MenuConfigVersion,
};

// Debounced input snapshot passed to the controller on each tick.
// - centerDown: level signal for long-press detection
// - action: edge-triggered high-level joystick action for navigation
struct InputSample
{
  bool centerDown;                    // true while the center button is held
  asap::input::JoyAction action;      // debounced directional/click edge
};

// Declarative navigation support
// The navigation graph below is defined in this header to allow tweaking the
// page arborescence without modifying source files. Each node can:
//  - declare children (for menus with selection)
//  - declare a back action to its parent (e.g., Left or Up)
//  - declare which actions confirm/enter (bitmask), and whether confirmation
//    goes to the selected child or a fixed target state
//  - attach small hooks for per-page adjustments (e.g., change selection or ID)

enum class ConfirmBehavior : uint8_t
{
  None,
  EnterSelectedChild,
  GoToTarget,
};

class UIController
{
 public:
  explicit UIController(asap::display::DetectorDisplay& display);

  // Advance UI state machine and render.
  // Call at a fixed cadence with current time and input sample.
  void onTick(uint32_t nowMs, const InputSample& sample);

  // External signal hooks
  void setAnomalyStrength(uint8_t percent);  // 0..100 (legacy bar fill)
  void feedTrackingRssi(int16_t rssiDbm);    // EMA input (dBm)
  // New anomaly HUD inputs used by DetectorDisplay::drawAnomalyIndicators:
  // - setAnomalyExposure: arc progress for current revolution (0..100%)
  // - setAnomalyStage: roman stage label (0 none/-, 1 I, 2 II, 3 III)
  void setAnomalyExposure(uint8_t rad, uint8_t therm, uint8_t chem, uint8_t psy);
  void setAnomalyStage(uint8_t rad, uint8_t therm, uint8_t chem, uint8_t psy);

  // For testing/inspection
  State state() const { return state_; }
  uint8_t trackingId() const { return trackingId_; }

 private:
  // Hook signatures for declarative pages
  using RenderHook = void (*)(UIController&);
  using ActionHook = void (*)(UIController&, asap::input::JoyAction);

  struct PageNode
  {
    State id;                   // page identifier
    State parent;               // parent page (for back navigation)
    const State* children;      // list of child pages (menu entries)
    uint8_t childCount;         // number of children
    asap::input::JoyAction backAction;  // action that returns to parent (or Neutral for none)
    uint8_t confirmMask;        // bitmask of actions that confirm/enter
    ConfirmBehavior confirm;    // how to interpret confirmation
    State confirmTarget;        // used when confirm == GoToTarget
    ActionHook onAction;        // optional per-page action hook (selection/ID changes)
    RenderHook render;          // page render function
  };

  // (removed) actionBit moved to free function at namespace scope

  // Core driver
  void render();
  void navigate(asap::input::JoyAction action);
  static const PageNode* findPage(State id);

  // Page hooks (declared here, defined in .cpp)
  static void RenderMenuRoot(UIController& self);
  static void RenderMenuAnomaly(UIController& self);
  static void RenderMenuTracking(UIController& self);
  static void RenderMenuConfig(UIController& self);
  static void RenderMainAnomaly(UIController& self);
  static void RenderMainTracking(UIController& self);

  static void ActionMenuRoot(UIController& self, asap::input::JoyAction action);
  static void ActionMenuTracking(UIController& self, asap::input::JoyAction action);
  static void ActionNoop(UIController& self, asap::input::JoyAction action);

  // Config submenu hooks
  static void RenderMenuConfigList(UIController& self);
  static void ActionMenuList(UIController& self, asap::input::JoyAction action);
  static void RenderConfigInvertX(UIController& self);
  static void RenderConfigInvertY(UIController& self);
  static void RenderConfigRotate(UIController& self);
  static void RenderConfigRssi(UIController& self);
  static void RenderConfigVersion(UIController& self);
  static void ActionConfigToggleX(UIController& self, asap::input::JoyAction action);
  static void ActionConfigToggleY(UIController& self, asap::input::JoyAction action);
  static void ActionConfigToggleRotate(UIController& self, asap::input::JoyAction action);

  asap::display::DetectorDisplay& display_;
  State state_;
  uint8_t selectedIndex_;  // root menu selection 0..2
  uint8_t trackingId_;
  int16_t rssiAvg_;        // EMA accumulator (in dBm)
  bool rssiInit_;
  uint8_t anomalyStrength_;
  // Per-channel anomaly state for the new HUD
  uint8_t anomalyRad_;    // 0..100 arc progress (radiation)
  uint8_t anomalyTherm_;  // 0..100 arc progress (thermal)
  uint8_t anomalyChem_;   // 0..100 arc progress (chemical)
  uint8_t anomalyPsy_;    // 0..100 arc progress (psy)
  uint8_t stageRad_;      // 0..3 stage → -, I, II, III
  uint8_t stageTherm_;    // 0..3 stage → -, I, II, III
  uint8_t stageChem_;     // 0..3 stage → -, I, II, III
  uint8_t stagePsy_;      // 0..3 stage → -, I, II, III

  // Config flags (volatile; persistence TBD)
  // These flags are toggled via the Config submenu. They are intentionally
  // volatile for now; persistence will be added through a small settings API.
  bool invertX_ = false;       // swap LEFT/RIGHT actions
  bool invertY_ = false;       // swap UP/DOWN actions
  bool rotateDisplay_ = false; // apply 180° rotation (U8g2 + native)

  // Long press gating
  bool firstActionDone_;   // becomes true after initial long-press
  bool centerPrev_;        // last sampled level for center button
  uint32_t pressStartMs_;  // timestamp when center transitioned to down
  static constexpr uint32_t kLongPressMs = 1000;  // confirmed requirement

  /*
   Developer Guide: Adding or Modifying Pages

   Goal: Update the page arborescence by editing this header and adding small
   hooks in UIController.cpp, without touching core navigation code.

   Steps:
   1) Declare hooks here (already patterned below):
        - Render hook:   static void RenderMyPage(UIController&);
        - Action hook:   static void ActionMyPage(UIController&, asap::input::JoyAction);
      Then implement them in UIController.cpp.

   2) If your page is a new menu item under an existing parent, add it to the
      parent’s children array (e.g., extend kRootChildren) and adjust childCount.

   3) Add a PageNode entry in kPages with:
        - id, parent
        - children + childCount (for menus; nullptr/0 for leaf pages)
        - backAction (e.g., Left or Up; Neutral if none)
        - confirmMask (bitwise OR of ActionBit(Right) | ActionBit(Click) etc.)
        - confirm behavior: EnterSelectedChild or GoToTarget (+ confirmTarget)
        - onAction hook (or ActionNoop)
        - render hook

   4) For a main (operational HUD) page, typically set:
        backAction = Neutral, confirmMask = 0, confirm = None,
        onAction = ActionNoop, render = your main-page renderer.

   Example (illustrative only):
     // Hooks (declare here; implement in .cpp)
     // static void RenderMenuFoo(UIController&);
     // static void ActionMenuFoo(UIController&, asap::input::JoyAction);
     // static void RenderMainFoo(UIController&);

     // Add child to root menu
     // inline constexpr State kRootChildren[] = { State::MenuAnomaly, State::MenuTracking, State::MenuConfig, State::MenuFoo };

     // Add nodes
     // { State::MenuFoo, State::MenuRoot, nullptr, 0,
     //   asap::input::JoyAction::Left,
     //   static_cast<uint8_t>(ActionBit(asap::input::JoyAction::Right) | ActionBit(asap::input::JoyAction::Click)),
     //   ConfirmBehavior::GoToTarget, State::MainFoo,
     //   &UIController::ActionMenuFoo, &UIController::RenderMenuFoo },
     // { State::MainFoo, State::MainFoo, nullptr, 0,
     //   asap::input::JoyAction::Neutral, 0, ConfirmBehavior::None, State::MainFoo,
     //   &UIController::ActionNoop, &UIController::RenderMainFoo },

   This keeps Up/Down scrolling driven by childCount and all transitions
   declaratively defined here.
  */

  // Declarative navigation graph data
  static inline constexpr State kRootChildren[3] = {
      State::MenuAnomaly, State::MenuTracking, State::MenuConfig,
  };

  static inline constexpr State kConfigChildren[5] = {
      State::MenuConfigInvertX,
      State::MenuConfigInvertY,
      State::MenuConfigRotate,
      State::MenuConfigRssiCal,
      State::MenuConfigVersion,
  };

  static inline constexpr PageNode kPages[] = {
      // Root menu: Up/Down adjust selection; Right/Click enter selected child
      {
          State::MenuRoot,
          /*parent=*/State::MenuRoot,  // self parent sentinel (unused for root)
          /*children=*/kRootChildren,
          /*childCount=*/static_cast<uint8_t>(sizeof(kRootChildren) / sizeof(kRootChildren[0])),
          /*backAction=*/asap::input::JoyAction::Neutral,
          /*confirmMask=*/static_cast<uint8_t>(ActionBit(asap::input::JoyAction::Right) |
                                              ActionBit(asap::input::JoyAction::Click)),
          /*confirm=*/ConfirmBehavior::EnterSelectedChild,
          /*confirmTarget=*/State::MenuRoot,
          /*onAction=*/&UIController::ActionMenuRoot,
          /*render=*/&UIController::RenderMenuRoot,
      },
      // Anomaly menu page: Left back, Right/Click confirm -> MainAnomaly
      {
          State::MenuAnomaly,
          /*parent=*/State::MenuRoot,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Left,
          /*confirmMask=*/static_cast<uint8_t>(ActionBit(asap::input::JoyAction::Right) |
                                              ActionBit(asap::input::JoyAction::Click)),
          /*confirm=*/ConfirmBehavior::GoToTarget,
          /*confirmTarget=*/State::MainAnomaly,
          /*onAction=*/&UIController::ActionNoop,
          /*render=*/&UIController::RenderMenuAnomaly,
      },
      // Tracking menu page: Up back, Left/Right adjust ID, Click confirm -> MainTracking
      {
          State::MenuTracking,
          /*parent=*/State::MenuRoot,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Up,
          /*confirmMask=*/ActionBit(asap::input::JoyAction::Click),
          /*confirm=*/ConfirmBehavior::GoToTarget,
          /*confirmTarget=*/State::MainTracking,
          /*onAction=*/&UIController::ActionMenuTracking,
          /*render=*/&UIController::RenderMenuTracking,
      },
      // Config menu page: Left back, no confirm target
      {
          State::MenuConfig,
          /*parent=*/State::MenuRoot,
          /*children=*/kConfigChildren,
          /*childCount=*/static_cast<uint8_t>(sizeof(kConfigChildren) / sizeof(kConfigChildren[0])),
          /*backAction=*/asap::input::JoyAction::Left,
          /*confirmMask=*/static_cast<uint8_t>(ActionBit(asap::input::JoyAction::Right) |
                                              ActionBit(asap::input::JoyAction::Click)),
          /*confirm=*/ConfirmBehavior::EnterSelectedChild,
          /*confirmTarget=*/State::MenuConfig,
          /*onAction=*/&UIController::ActionMenuList,
          /*render=*/&UIController::RenderMenuConfigList,
      },
      // Main pages (no navigation; rendered from external state data)
      {
          State::MainAnomaly,
          /*parent=*/State::MainAnomaly,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Neutral,
          /*confirmMask=*/0,
          /*confirm=*/ConfirmBehavior::None,
          /*confirmTarget=*/State::MainAnomaly,
          /*onAction=*/&UIController::ActionNoop,
          /*render=*/&UIController::RenderMainAnomaly,
      },
      {
          State::MainTracking,
          /*parent=*/State::MainTracking,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Neutral,
          /*confirmMask=*/0,
          /*confirm=*/ConfirmBehavior::None,
          /*confirmTarget=*/State::MainTracking,
          /*onAction=*/&UIController::ActionNoop,
          /*render=*/&UIController::RenderMainTracking,
      },
      // Config leaf pages
      {
          State::MenuConfigInvertX,
          /*parent=*/State::MenuConfig,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Left,
          /*confirmMask=*/0,
          /*confirm=*/ConfirmBehavior::None,
          /*confirmTarget=*/State::MenuConfigInvertX,
          /*onAction=*/&UIController::ActionConfigToggleX,
          /*render=*/&UIController::RenderConfigInvertX,
      },
      {
          State::MenuConfigInvertY,
          /*parent=*/State::MenuConfig,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Left,
          /*confirmMask=*/0,
          /*confirm=*/ConfirmBehavior::None,
          /*confirmTarget=*/State::MenuConfigInvertY,
          /*onAction=*/&UIController::ActionConfigToggleY,
          /*render=*/&UIController::RenderConfigInvertY,
      },
      {
          State::MenuConfigRotate,
          /*parent=*/State::MenuConfig,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Left,
          /*confirmMask=*/0,
          /*confirm=*/ConfirmBehavior::None,
          /*confirmTarget=*/State::MenuConfigRotate,
          /*onAction=*/&UIController::ActionConfigToggleRotate,
          /*render=*/&UIController::RenderConfigRotate,
      },
      {
          State::MenuConfigRssiCal,
          /*parent=*/State::MenuConfig,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Left,
          /*confirmMask=*/0,
          /*confirm=*/ConfirmBehavior::None,
          /*confirmTarget=*/State::MenuConfigRssiCal,
          /*onAction=*/&UIController::ActionNoop,
          /*render=*/&UIController::RenderConfigRssi,
      },
      {
          State::MenuConfigVersion,
          /*parent=*/State::MenuConfig,
          /*children=*/nullptr,
          /*childCount=*/0,
          /*backAction=*/asap::input::JoyAction::Left,
          /*confirmMask=*/0,
          /*confirm=*/ConfirmBehavior::None,
          /*confirmTarget=*/State::MenuConfigVersion,
          /*onAction=*/&UIController::ActionNoop,
          /*render=*/&UIController::RenderConfigVersion,
      },
  };
};

}  // namespace asap::ui
