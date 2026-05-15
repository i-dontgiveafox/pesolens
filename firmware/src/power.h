// pesos firmware — power module
// Deep sleep, button wake, and battery monitoring.

#pragma once

#include <esp_sleep.h>

// Configure GPIO ext0 wake source on the button pin.
void power_configure_wake_source();

// Halt and enter deep sleep. Does NOT return.
void power_enter_deep_sleep();

// Read VBAT through optional divider; returns 0.0 if unwired.
float power_read_battery_voltage();

// Why did we wake up? (button, first-boot, timer, etc.)
esp_sleep_wakeup_cause_t power_get_wake_cause();

// True while the button is currently held down (active-low). Useful for debugging.
bool power_button_is_pressed();

// Decode the latest wake cause to a readable string and print it over Serial.
void power_print_wake_reason();
