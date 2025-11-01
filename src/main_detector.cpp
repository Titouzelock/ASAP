#include <Arduino.h>

#include <asap/display/DetectorDisplay.h>

using asap::display::DetectorDisplay;
using asap::display::DisplayPins;

namespace {

constexpr DisplayPins kDisplayPins = {
    .chipSelect = PA4,
    .dataCommand = PA3,
    .reset = PA2,
};

constexpr uint32_t kHeartbeatIntervalMs = 250;

DetectorDisplay detectorDisplay(kDisplayPins);
uint32_t lastHeartbeat = 0;

}  // namespace

void setup() {
  SerialUSB.begin(115200);
  delay(500);
  SerialUSB.println("[ASAP] Detector firmware booted.");

  if (detectorDisplay.begin()) {
    detectorDisplay.drawBootScreen(ASAP_VERSION);
  } else {
    SerialUSB.println("[ASAP] Display initialization failed.");
  }
}

void loop() {
  const uint32_t now = millis();
  if (now - lastHeartbeat >= kHeartbeatIntervalMs) {
    lastHeartbeat = now;
    SerialUSB.println("tick");
    detectorDisplay.drawHeartbeatFrame(now);
  }
}
