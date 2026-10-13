// pesos firmware - central configuration
// All pin assignments, thresholds, and tunables live here.
// Edit one file to retune the device.

#pragma once

#include <stdint.h>

// GPIO assignments
// Tactile push button on XIAO silkscreen D4: active-low, INPUT_PULLUP,
// also used as the deep-sleep EXT0 wake source. D4 maps to GPIO5.
#define BUTTON_PIN          5

// I2S audio out -> MAX98357A -> bone-conduction speaker.
// XIAO ESP32-S3 silkscreen labels map as D9=GPIO8, D8=GPIO7, D7=GPIO44.
#define I2S_DIN             8
#define I2S_BCLK            7
#define I2S_LRC             44

// Camera
#define CAMERA_FRAME_SIZE   FRAMESIZE_QVGA
#define CAMERA_JPEG_QUALITY 12
#define CAMERA_FB_COUNT     1

// Model
#define MODEL_INPUT_SIZE    160
#define MODEL_NUM_CLASSES   4
#define CONF_THRESHOLD      0.25f
#define IOU_THRESHOLD       0.45f

// Tensor arena: lives in PSRAM.
#define TENSOR_ARENA_SIZE   (4 * 1024 * 1024)

// USB/demo scan loop. While enabled, the firmware scans automatically instead
// of waiting for the D4/GPIO5 wake button.
#define AUTO_SCAN_INTERVAL_MS        3000
#define AUTO_SCAN_AFTER_DETECT_MS    5000

// Class definitions
// Must match the current PesoLens model order: 100, 1000, 50, 500.
static const uint16_t DENOMINATIONS[MODEL_NUM_CLASSES] = {
    100, 1000, 50, 500
};

static const char* CLASS_NAMES[MODEL_NUM_CLASSES] = {
    "100", "1000", "50", "500"
};

// Power
#define WAKE_GPIO_PIN       BUTTON_PIN
#define WAKE_GPIO_LEVEL     0

// Optional VBAT divider
#define BATTERY_ADC_PIN     -1
