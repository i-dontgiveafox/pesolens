#include <Arduino.h>

#include "audio.h"

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 5000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(500);

    Serial.println();
    Serial.println("=== audio probe boot ===");
    if (!audio_init()) {
        Serial.println("[audio_probe] audio_init failed");
        return;
    }
    Serial.println("[audio_probe] playing beep every second");
}

void loop() {
    static uint32_t count = 0;
    Serial.printf("[audio_probe] beep %lu\n", (unsigned long)count++);
    audio_play_clip("beep");
    delay(1000);
}
