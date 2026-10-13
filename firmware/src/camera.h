// pesos firmware — camera module
// Owns OV2640/OV3660 init, single-shot capture, and model-size int8 preprocessing.

#pragma once

#include <stdint.h>
#include "esp_camera.h"

// Boot-time PSRAM availability check. Returns false if PSRAM is missing —
// the rest of the pipeline cannot run without it.
bool psram_check();

// One-time camera init. Safe to call once after wake-from-sleep.
bool camera_init();

// Capture one frame. Caller MUST call camera_release(fb) when done.
camera_fb_t* camera_capture();

// Center-crop + resize the frame to MODEL_INPUT_SIZE × MODEL_INPUT_SIZE
// and write int8 normalized pixels (NHWC, RGB) into out_buf. out_buf must
// be at least MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3 bytes.
// Returns true on success, false on null/format/length mismatch — main.cpp
// must check the return and skip inference on failure.
bool camera_preprocess_for_model(camera_fb_t* fb, int8_t* out_buf);

// Return the frame buffer to the camera driver pool.
void camera_release(camera_fb_t* fb);

// Debug helper: convert one frame to JPEG and stream it as base64 over Serial.
// Used only by capture/debug builds.
bool camera_dump_jpeg_to_serial(camera_fb_t* fb);
