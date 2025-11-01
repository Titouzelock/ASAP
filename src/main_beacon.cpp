#include <Arduino.h>

void setup() {
  SerialUSB.begin(115200);
  delay(500);
  SerialUSB.println("[ASAP] Beacon firmware booted.");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    SerialUSB.println("beacon ping");
  }
}
