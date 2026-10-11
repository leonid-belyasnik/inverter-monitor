
#include <Arduino.h>

void setup()
{
    Serial.begin(115200);

    const unsigned long start = millis();
    while (!Serial && millis() - start < 10000)
    {
        delay(100);
    }

    Serial.println("HomePowerGuard ESP32 firmware starting");
}

void loop()
{
    Serial.println("alive");
    delay(2000);
}
