// GPIO probe: find which pin a button is wired to, and to which rail.
// Every XIAO D-pad pin is sampled twice per loop: once with the internal
// pull-up (a button to GND reads LOW) and once with the pull-down (a button
// to 3V3 reads HIGH). Any change is printed, and the heartbeat prints all
// levels so a pin that is stuck from boot is visible too.
// Build: pio run -e gpio_probe -t upload, then open the monitor at 115200.
#include <Arduino.h>

struct ProbePin { const char* label; uint8_t gpio; };

// XIAO ESP32-S3 silkscreen -> GPIO. D0..D10 are all off the camera bus.
static const ProbePin kPins[] = {
    {"D0", 1}, {"D1", 2}, {"D2", 3}, {"D3", 4}, {"D4", 5}, {"D5", 6},
    {"D6", 43}, {"D7", 44}, {"D8", 7}, {"D9", 8}, {"D10", 9},
};
static const size_t kNumPins = sizeof(kPins) / sizeof(kPins[0]);

// Bit 0: level with pull-up. Bit 1: level with pull-down.
// Floating pin: up=1, down=0. Tied to GND: 0,0. Tied to 3V3: 1,1.
static int g_last[kNumPins];

static int sample(uint8_t gpio) {
    pinMode(gpio, INPUT_PULLUP);
    delayMicroseconds(200);
    const int up = digitalRead(gpio);
    pinMode(gpio, INPUT_PULLDOWN);
    delayMicroseconds(200);
    const int down = digitalRead(gpio);
    pinMode(gpio, INPUT_PULLUP);
    return up | (down << 1);
}

static const char* describe(int s) {
    switch (s) {
        case 1:  return "open (floating)";
        case 0:  return "tied to GND";
        case 3:  return "tied to 3V3";
        default: return "odd (up=L, down=H)";
    }
}

static void print_all(const char* tag) {
    Serial.printf("[probe] %s:", tag);
    for (size_t i = 0; i < kNumPins; ++i) {
        Serial.printf(" %s=%s", kPins[i].label,
                      g_last[i] == 1 ? "open" : g_last[i] == 0 ? "GND" : g_last[i] == 3 ? "3V3" : "odd");
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    uint32_t deadline = millis() + 5000;
    while (!Serial && millis() < deadline) delay(10);
    delay(300);
    Serial.println("\n=== gpio probe v2 (pull-up + pull-down) ===");
    for (size_t i = 0; i < kNumPins; ++i) {
        g_last[i] = sample(kPins[i].gpio);
    }
    print_all("idle");
    Serial.println("[probe] press the button now");
}

void loop() {
    static uint32_t last_heartbeat = 0;
    for (size_t i = 0; i < kNumPins; ++i) {
        const int s = sample(kPins[i].gpio);
        if (s != g_last[i]) {
            Serial.printf("[probe] %s/GPIO%u: %s -> %s\n", kPins[i].label,
                          (unsigned)kPins[i].gpio, describe(g_last[i]), describe(s));
            g_last[i] = s;
        }
    }
    if (millis() - last_heartbeat > 10000) {
        last_heartbeat = millis();
        print_all("levels");
    }
    delay(5);
}
