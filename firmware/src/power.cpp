// pesos firmware — power module
// Deep sleep + EXT0 button wake on XIAO D4 / GPIO5 (RTC-IO capable on ESP32-S3).
//
// Power-down ordering (caller responsibility):
//   1. camera_deinit()     — releases DMA, frame buffers, and the camera clock.
//   2. audio_deinit()      — uninstalls the I2S driver and lets the amp idle.
//   3. inference teardown  — frees the tensor arena (lives in PSRAM).
//   4. power_enter_deep_sleep().
// Skipping any of these leaves peripherals clocked, which can add hundreds of
// microamps to deep-sleep current and defeats the whole point.

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "power.h"
#include "config.h"

void power_configure_wake_source() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Keep the internal pullup alive through deep sleep so a floating line
    // doesn't trigger a phantom wake. rtc_gpio_isolate() would save more
    // current but disables the wake source — don't use it on the wake pin.
    gpio_pullup_en((gpio_num_t)BUTTON_PIN);
    gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, WAKE_GPIO_LEVEL);
    Serial.println("[power] wake source: EXT0 on D4/GPIO5 (button, active-low)");
}

esp_sleep_wakeup_cause_t power_get_wake_cause() {
    return esp_sleep_get_wakeup_cause();
}

void power_enter_deep_sleep() {
    Serial.println("[power] entering deep sleep...");
    Serial.flush();
    esp_deep_sleep_start();
}

float power_read_battery_voltage() {
    // The Seeed XIAO ESP32-S3 does not expose VBAT on a default GPIO.
    // V1 returns a sentinel; wire an external divider and set BATTERY_ADC_PIN
    // in config.h to enable real readings.
    Serial.println("[power] battery voltage monitoring not configured (no VBAT pin on default XIAO)");
    return 0.0f;

    // Future implementation, once a divider (e.g. 100k:100k on a free GPIO) is wired:
    //   uint16_t raw = analogRead(BATTERY_ADC_PIN);
    //   float v_adc  = raw * 3.3f / 4095.0f;
    //   return v_adc * 2.0f; // multiply by divider ratio
}

bool power_button_is_pressed() {
    return digitalRead(BUTTON_PIN) == LOW;
}

void power_print_wake_reason() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const char* label;
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0:      label = "EXT0 (button)";      break;
        case ESP_SLEEP_WAKEUP_EXT1:      label = "EXT1";               break;
        case ESP_SLEEP_WAKEUP_TIMER:     label = "TIMER";              break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:  label = "TOUCHPAD";           break;
        case ESP_SLEEP_WAKEUP_ULP:       label = "ULP";                break;
        case ESP_SLEEP_WAKEUP_GPIO:      label = "GPIO";               break;
        case ESP_SLEEP_WAKEUP_UART:      label = "UART";               break;
        case ESP_SLEEP_WAKEUP_UNDEFINED: label = "UNDEFINED (cold boot / reset)"; break;
        default:                         label = "OTHER";              break;
    }
    Serial.printf("[power] wake reason: %s\n", label);
}
