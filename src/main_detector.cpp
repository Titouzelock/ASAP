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
  SerialUSB.begin(115200);  // enable USB CDC logging
  delay(500);               // allow host to settle before printing
  SerialUSB.println("[ASAP] Detector firmware booted.");

  if (detectorDisplay.begin()) {
    detectorDisplay.drawBootScreen(ASAP_VERSION);  // show boot splash
  } else {
    SerialUSB.println("[ASAP] Display initialization failed.");
  }
}

void loop()
{
  const uint32_t now = millis();  // current uptime snapshot
  if (now - lastHeartbeat >= kHeartbeatIntervalMs)
  {
    lastHeartbeat = now;
    SerialUSB.println("tick");  // placeholder heartbeat log

    // TODO: Read actual joystick hardware states.
    const bool centerDown = false;  // placeholder until hardware driver is wired
    const asap::input::JoyAction action = asap::input::JoyAction::Neutral;
    ui.onTick(now, {centerDown, action});
  }
}
