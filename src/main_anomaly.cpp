#include <Arduino.h>

void setup()
{
  SerialUSB.begin(115200);
  delay(500);
  SerialUSB.println("[ASAP] Anomaly node booted.");
}

void loop()
{
  static uint32_t last = 0;
  if (millis() - last > 1500)
  {
    last = millis();
    SerialUSB.println("anomaly pulse");
  }
}
