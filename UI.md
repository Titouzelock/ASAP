UI Guide

Display & UI Contracts
- Menu/input mapping (Detector):
  - First-action gating: the first interaction must be a 1000 ms long-press on the center button to enter the menu.
  - Root: ANOMALY, TRACKING, PLAYER DATA, CONFIG. Up/Down to move. Right/Click to enter.
  - ANOMALY page: Left = back; Right/Click = set Main Anomaly and exit.
  - TRACKING page: Up = back; Left/Right = change Track ID; Click = set Main Tracking and exit.
  - CONFIG page: Left = back; contains toggles and info items.
- HUDs:
  - Anomaly: 15 px high full-width progress bar near bottom.
  - Tracking: shows TRACK <ID> and RSSI <avg>dBm with EMA smoothing.
- UI navigation graph:
  - The page arborescence and transitions are declared in `lib/asap_ui/src/asap/ui/UIController.h` via a `PageNode` table (`kPages`).
  - Up/Down scrolling updates `selectedIndex_` through the page's action hook using the graph's `childCount` (no hard-coded item counts).
  - Confirmation/back behavior is defined declaratively: confirm mask + `EnterSelectedChild` or `GoToTarget`, and a single `backAction` per page.
  - To add/modify pages: add a render hook and optional action hook in `UIController.cpp`, then wire the node in `kPages` (header-only change for structure).
  - Config submenu is a paged list under `MenuConfig` with items: Invert X Joystick, Invert Y Joystick, Rotate Display, RSSI Calibration (placeholder), Version Display. Each item has its own render/action hook; toggles happen on Right/Click and display ON/OFF.
  - Selection policy: when entering a menu/list page, selection resets to the first item to keep navigation deterministic.

Player Data Page
- Placement: root menu item "PLAYER DATA", positioned before "CONFIG".
- Behavior: read-only view of persistent player data stored in flash.
- Scrolling: 3-line window; Down advances by 3 lines, Up moves back by 3 (no wrap at top).
- Exit: Click or Left returns to the root menu; Down at the end also returns to the menu.
  - "PLAYER DATA"
  - "Version <n>"
  - Compact pairs: "FIRE <n> | PSY <n>", "RAD <n> | CHEM <n>", "ARM <n> | FAC <n>"
  - "MONEY <rubles>" where rubles = `money_units * 100`
  - Wrapped description text:
    - First segment shows as-is; continuation segments are prefixed with "> "
    - CR/LF produce new logical lines; consecutive CR/LF yield visible empty lines
- UI tag: the top-right "MENU" tag is hidden on this page for visual focus.

Implemented Toggles
- Joystick inversion (X/Y) is applied at the input layer in `UIController::onTick`.

Control variables (from `UIController`)
- `setAnomalyExposure(rad, therm, chem, psy)` for arc progress 0..100%.
- `setAnomalyStage(rad, therm, chem, psy)` for stages 0..3 (rendered as -, I, II, III).

