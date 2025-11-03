#include <Arduino.h>  // core Arduino API for STM32 targets

#include <asap/display/DetectorDisplay.h>  // SSD1322 display driver abstraction
#include <asap/input/Joystick.h>          // JoyAction for debug page
#include <asap/ui/UIController.h>         // UI state machine

using asap::display::DetectorDisplay;
using asap::display::DisplayPins;

namespace
{

// SPI wiring for the SSD1322 module on the detector PCB.
constexpr DisplayPins kDisplayPins = {
    .chipSelect = PA4,
    .dataCommand = PA3,
    .reset = PA2,
};

constexpr uint32_t kHeartbeatIntervalMs = 250;  // refresh cadence for UI/serial

DetectorDisplay detectorDisplay(kDisplayPins);  // global display instance
uint32_t lastHeartbeat = 0;                     // last time the heartbeat ran
asap::ui::UIController ui(detectorDisplay);     // UI controller

}  // namespace

void setup()
{
  if (detectorDisplay.begin()) {
    detectorDisplay.drawBootScreen(ASAP_VERSION);  // show boot splash
  } else {
    // no-op: logging disabled to save flash (USB CDC removed)
  }
}

void loop()
{
  const uint32_t now = millis();  // current uptime snapshot
  if (now - lastHeartbeat >= kHeartbeatIntervalMs)
  {
    lastHeartbeat = now;
    // heartbeat log removed (USB CDC disabled)

    // TODO: Read actual joystick hardware states.
    const bool centerDown = false;  // placeholder until hardware driver is wired
    const asap::input::JoyAction action = asap::input::JoyAction::Neutral;
    ui.onTick(now, {centerDown, action});
  }
}
