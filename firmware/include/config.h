// pesos firmware - central configuration
// All pin assignments, thresholds, and tunables live here.
// Edit one file to retune the device.

#pragma once

#include <stdint.h>

// GPIO assignments
// Tactile push button on XIAO silkscreen D4: active-low, INPUT_PULLUP,
// also used as the deep-sleep EXT0 wake source. D4 maps to GPIO5.
#define BUTTON_PIN          5

// Manual capture latching switch on XIAO silkscreen D6. The installed
// XIAO_ESP32S3 Arduino variant maps D6 to GPIO43. Wire the switch so OFF is
// open/HIGH and ON connects D6 to GND/LOW. Firmware triggers on the OFF→ON
// transition and requires a return to OFF before another scan.
// Standalone auto-run builds override this to -1 and reuse D6/GPIO43 as the
// ready/status LED pin.
#ifndef CAPTURE_BUTTON_PIN
#define CAPTURE_BUTTON_PIN  43
#endif
#define MANUAL_CAPTURE_DELAY_MS 3000

// Ready indicator LED. Debug/manual builds keep this on D5/GPIO6 because
// D6/GPIO43 is the manual capture button. The standalone deployment env
// overrides this to D6/GPIO43.
// If the LED is wired to 3V3 instead, swap these two levels.
#ifndef READY_LED_PIN
#define READY_LED_PIN       6
#endif
#ifndef READY_LED_XIAO_D_PIN
#define READY_LED_XIAO_D_PIN 5
#endif
#define READY_LED_ON_LEVEL  1
#define READY_LED_OFF_LEVEL 0
#define READY_LED_BLINK_COUNT 3
#define READY_LED_BLINK_MS    1000

#if defined(PESOS_MANUAL_CAPTURE) && READY_LED_PIN == CAPTURE_BUTTON_PIN
#error "READY_LED_PIN conflicts with CAPTURE_BUTTON_PIN in manual capture mode"
#endif

// I2S audio out -> MAX98357A -> bone-conduction speaker.
// XIAO ESP32-S3 silkscreen labels map as D9=GPIO8, D8=GPIO7, D7=GPIO44.
#define I2S_DIN             8
#define I2S_BCLK            7
#define I2S_LRC             44

// Camera
#define CAMERA_FRAME_SIZE   FRAMESIZE_QVGA
#define CAMERA_JPEG_QUALITY 12
#define CAMERA_FB_COUNT     1
#define CAMERA_WARMUP_FRAMES 2

// Model
#define MODEL_INPUT_SIZE    256
#define MODEL_NUM_CLASSES   5
#define CONF_THRESHOLD      0.25f
#define IOU_THRESHOLD       0.45f
#define CLASS_AGNOSTIC_NMS  1
// Multi-bill mode. Set to 0 so every post-NMS detection is kept, counted, and
// summed into the announced total. Double-counting a single physical bill is
// already prevented by CLASS_AGNOSTIC_NMS above: overlapping boxes (even across
// denominations) suppress each other, so one bill yields one box. Set back to 1
// to force single-bill behavior (keep only the strongest box) if false extra
// detections become a problem in live testing.
#define SINGLE_BILL_MODE    0

// Tensor arena: lives in PSRAM.
#define TENSOR_ARENA_SIZE   (4 * 1024 * 1024)

// USB/demo scan loop. While enabled, the firmware scans automatically instead
// of waiting for the D4/GPIO5 wake button. The LED stays on during this ready
// window, then turns off for capture, inference, and audio playback.
#ifndef AUTO_SCAN_READY_MS
#define AUTO_SCAN_READY_MS           5000
#endif

// Capture-only debug/image-collection loop. Used by the auto JPEG dump build
// so images can be collected on the host without pressing D6.
#ifndef AUTO_CAPTURE_INTERVAL_MS
#define AUTO_CAPTURE_INTERVAL_MS     8000
#endif

// Class definitions. A single table prevents denomination values and log names
// from drifting into different orders.
struct ModelClassInfo {
    uint16_t denomination;
    const char* name;
};

static constexpr ModelClassInfo MODEL_CLASSES[] = {
    {20, "20"},
    {50, "50"},
    {100, "100"},
    {500, "500"},
    {1000, "1000"},
};

static_assert(
    sizeof(MODEL_CLASSES) / sizeof(MODEL_CLASSES[0]) == MODEL_NUM_CLASSES,
    "MODEL_NUM_CLASSES must match MODEL_CLASSES");

// Power
#define WAKE_GPIO_PIN       BUTTON_PIN
#define WAKE_GPIO_LEVEL     0

// Optional VBAT divider
#define BATTERY_ADC_PIN     -1
