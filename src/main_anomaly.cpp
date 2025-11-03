#include <Arduino.h>

void setup()
{
}

void loop()
{
  static uint32_t last = 0;
  if (millis() - last > 1500)
  {
    last = millis();
    // logging removed to save flash (USB CDC disabled)
  }
}
