#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 5000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(500);
    Serial.println();
    Serial.println("=== serial probe boot ===");
    Serial.println("If you can read this, USB serial logging works on COM3.");
}

void loop() {
    static uint32_t counter = 0;
    Serial.printf("[serial_probe] alive %lu ms count=%lu\n",
                  (unsigned long)millis(),
                  (unsigned long)counter++);
    Serial.flush();
    delay(1000);
}
