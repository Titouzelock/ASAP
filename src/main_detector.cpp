#include <Arduino.h>

void setup() {
  // Ce code s'exécute une fois au démarrage
  SerialUSB.begin(115200);
  delay(500);
  SerialUSB.println("[ASAP] Detector firmware booted.");
}

void loop() {
  // Ce code tourne en boucle
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    SerialUSB.println("tick");
  }
}
