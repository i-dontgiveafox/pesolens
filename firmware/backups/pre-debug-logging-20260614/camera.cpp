// pesos firmware — camera module
// Owns OV2640/OV3660 init, single-shot capture, and model-size int8 preprocessing.
//
// Pipeline: OV2640/OV3660 -> RGB565 QVGA (320x240)
//        -> center-crop to 240x240
//        -> bilinear resize to MODEL_INPUT_SIZE x MODEL_INPUT_SIZE RGB888
//        -> int8 quantize (uint8 - 128)
//        -> MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3 tensor input (NHWC, RGB).

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include <stdlib.h>
#include "esp_camera.h"
#include "img_converters.h"

#include "camera.h"
#include "camera_pins.h"
#include "config.h"

// ---------------- PSRAM ----------------

bool psram_check() {
    if (!psramFound()) {
        Serial.println("[cam] ERROR: PSRAM not detected");
        return false;
    }
    size_t total_psram = ESP.getPsramSize();
    size_t free_psram  = ESP.getFreePsram();
    Serial.printf("[cam] PSRAM total=%u bytes, free=%u bytes\n",
                  (unsigned)total_psram, (unsigned)free_psram);

    const size_t required = 6u * 1024u * 1024u;  // 4 MB arena + 1 MB camera + headroom
    if (free_psram < required) {
        Serial.printf("[cam] ERROR: only %u bytes free PSRAM, need >= %u\n",
                      (unsigned)free_psram, (unsigned)required);
        return false;
    }
    Serial.println("[cam] PSRAM ok");
    return true;
}

// ---------------- Init ----------------

bool camera_init() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size   = CAMERA_FRAME_SIZE;          // FRAMESIZE_QVGA (320x240)
    config.jpeg_quality = CAMERA_JPEG_QUALITY;        // unused for raw RGB
    config.fb_count     = CAMERA_FB_COUNT;            // 1
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[cam] ERROR: esp_camera_init failed 0x%x\n", err);
        return false;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s != nullptr) {
        s->set_whitebal(s, 1);   // auto white balance enable
        s->set_awb_gain(s, 1);   // auto white balance gain enable
        s->set_wb_mode(s, 0);    // 0 = auto
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 1);
        s->set_gain_ctrl(s, 1);
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
    } else {
        Serial.println("[cam] WARNING: sensor handle null, using defaults");
    }

    delay(200);
    for (int i = 0; i < CAMERA_WARMUP_FRAMES; ++i) {
        camera_fb_t* warmup = esp_camera_fb_get();
        if (warmup != nullptr) {
            esp_camera_fb_return(warmup);
            Serial.printf("[cam] warmup frame %d/%d ok\n", i + 1, CAMERA_WARMUP_FRAMES);
        } else {
            Serial.printf("[cam] WARNING: warmup frame %d/%d failed\n", i + 1, CAMERA_WARMUP_FRAMES);
        }
        delay(50);
    }

    Serial.println("[cam] init ok (RGB565 QVGA 320x240)");
    return true;
}

// ---------------- Capture ----------------

camera_fb_t* camera_capture() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
        Serial.println("[cam] ERROR: esp_camera_fb_get returned null");
        return nullptr;
    }
    Serial.printf("[cam] capture %ux%u (%u bytes, fmt=%d)\n",
                  (unsigned)fb->width, (unsigned)fb->height,
                  (unsigned)fb->len, (int)fb->format);
    return fb;
}

void camera_release(camera_fb_t* fb) {
    if (fb != nullptr) {
        esp_camera_fb_return(fb);
    }
}

bool camera_dump_jpeg_to_serial(camera_fb_t* fb) {
    if (fb == nullptr) {
        Serial.println("[frame] ERROR: cannot dump null frame");
        return false;
    }

    uint8_t* jpg_buf = nullptr;
    size_t jpg_len = 0;
    const bool ok = fmt2jpg(
        fb->buf, fb->len, fb->width, fb->height, fb->format,
        85, &jpg_buf, &jpg_len);
    if (!ok || jpg_buf == nullptr || jpg_len == 0) {
        Serial.println("[frame] ERROR: fmt2jpg failed");
        if (jpg_buf != nullptr) {
            free(jpg_buf);
        }
        return false;
    }

    Serial.printf("[frame] jpeg bytes=%u\n", (unsigned)jpg_len);
    Serial.println("[frame] JPEG_BASE64_BEGIN");

    const size_t in_chunk = 768;
    uint8_t out_chunk[1100];
    for (size_t off = 0; off < jpg_len; off += in_chunk) {
        const size_t n = (jpg_len - off < in_chunk) ? (jpg_len - off) : in_chunk;
        size_t out_len = 0;
        const int err = mbedtls_base64_encode(
            out_chunk, sizeof(out_chunk), &out_len, jpg_buf + off, n);
        if (err != 0) {
            Serial.printf("[frame] ERROR: base64 encode failed %d\n", err);
            free(jpg_buf);
            return false;
        }
        Serial.write(out_chunk, out_len);
        Serial.println();
    }

    Serial.println("[frame] JPEG_BASE64_END");
    free(jpg_buf);
    return true;
}

// ---------------- Preprocess ----------------

// Decode one RGB565 pixel into 8-bit R/G/B channels.
// Big-endian byte order is what esp32-camera produces in its RGB565 buffer.
static inline void rgb565_to_rgb888(uint16_t px, uint8_t& r, uint8_t& g, uint8_t& b) {
    // RGB565 layout: RRRRR GGGGGG BBBBB
    uint8_t r5 = (px >> 11) & 0x1F;
    uint8_t g6 = (px >> 5)  & 0x3F;
    uint8_t b5 =  px        & 0x1F;
    // Scale to 8-bit with rounding.
    r = (uint8_t)((r5 * 255 + 15) / 31);
    g = (uint8_t)((g6 * 255 + 31) / 63);
    b = (uint8_t)((b5 * 255 + 15) / 31);
}

// Bilinear resample of a center-cropped square region of an RGB565 source
// (width=src_w, height=src_h) into a dst_size x dst_size int8 RGB888 buffer.
// The source is interpreted as big-endian RGB565 (esp32-camera default).
static void bilinear_crop_resize_quantize(const uint8_t* src_bytes,
                                          int src_w, int src_h,
                                          int crop_x0, int crop_y0,
                                          int crop_size,
                                          int dst_size,
                                          int8_t* dst) {
    // Source row stride in bytes (RGB565 = 2 bytes per pixel).
    const int src_stride = src_w * 2;

    // Map dst pixel center -> source pixel coordinate (in cropped region).
    // Use the standard formula: src = (dst + 0.5) * scale - 0.5
    const float scale = (float)crop_size / (float)dst_size;

    for (int dy = 0; dy < dst_size; ++dy) {
        float fy = (dy + 0.5f) * scale - 0.5f + (float)crop_y0;
        if (fy < 0.0f) fy = 0.0f;
        if (fy > (float)(src_h - 1)) fy = (float)(src_h - 1);
        int y0 = (int)fy;
        int y1 = y0 + 1;
        if (y1 > src_h - 1) y1 = src_h - 1;
        float wy = fy - (float)y0;

        for (int dx = 0; dx < dst_size; ++dx) {
            float fx = (dx + 0.5f) * scale - 0.5f + (float)crop_x0;
            if (fx < 0.0f) fx = 0.0f;
            if (fx > (float)(src_w - 1)) fx = (float)(src_w - 1);
            int x0 = (int)fx;
            int x1 = x0 + 1;
            if (x1 > src_w - 1) x1 = src_w - 1;
            float wx = fx - (float)x0;

            // Fetch the four neighbour RGB565 pixels (big-endian: hi byte first).
            const uint8_t* p00 = src_bytes + y0 * src_stride + x0 * 2;
            const uint8_t* p01 = src_bytes + y0 * src_stride + x1 * 2;
            const uint8_t* p10 = src_bytes + y1 * src_stride + x0 * 2;
            const uint8_t* p11 = src_bytes + y1 * src_stride + x1 * 2;

            uint16_t px00 = ((uint16_t)p00[0] << 8) | (uint16_t)p00[1];
            uint16_t px01 = ((uint16_t)p01[0] << 8) | (uint16_t)p01[1];
            uint16_t px10 = ((uint16_t)p10[0] << 8) | (uint16_t)p10[1];
            uint16_t px11 = ((uint16_t)p11[0] << 8) | (uint16_t)p11[1];

            uint8_t r00, g00, b00; rgb565_to_rgb888(px00, r00, g00, b00);
            uint8_t r01, g01, b01; rgb565_to_rgb888(px01, r01, g01, b01);
            uint8_t r10, g10, b10; rgb565_to_rgb888(px10, r10, g10, b10);
            uint8_t r11, g11, b11; rgb565_to_rgb888(px11, r11, g11, b11);

            float w00 = (1.0f - wx) * (1.0f - wy);
            float w01 = wx          * (1.0f - wy);
            float w10 = (1.0f - wx) * wy;
            float w11 = wx          * wy;

            float fr = r00 * w00 + r01 * w01 + r10 * w10 + r11 * w11;
            float fg = g00 * w00 + g01 * w01 + g10 * w10 + g11 * w11;
            float fb = b00 * w00 + b01 * w01 + b10 * w10 + b11 * w11;

            int ir = (int)(fr + 0.5f); if (ir < 0) ir = 0; if (ir > 255) ir = 255;
            int ig = (int)(fg + 0.5f); if (ig < 0) ig = 0; if (ig > 255) ig = 255;
            int ib = (int)(fb + 0.5f); if (ib < 0) ib = 0; if (ib > 255) ib = 255;

            // int8 symmetric quant: x_int8 = uint8 - 128
            int idx = (dy * dst_size + dx) * 3;
            dst[idx + 0] = (int8_t)(ir - 128);
            dst[idx + 1] = (int8_t)(ig - 128);
            dst[idx + 2] = (int8_t)(ib - 128);
        }
    }
}

bool camera_preprocess_for_model(camera_fb_t* fb, int8_t* out_buf) {
    if (fb == nullptr || out_buf == nullptr) {
        Serial.println("[cam] ERROR: preprocess called with null pointer");
        return false;
    }
    if (fb->format != PIXFORMAT_RGB565) {
        Serial.printf("[cam] ERROR: preprocess expected RGB565, got fmt=%d\n",
                      (int)fb->format);
        return false;
    }

    const int src_w = (int)fb->width;
    const int src_h = (int)fb->height;

    // Validate the buffer is large enough — RGB565 = 2 bytes/pixel.
    const size_t expected_bytes = (size_t)src_w * (size_t)src_h * 2;
    if (fb->len < expected_bytes) {
        Serial.printf("[cam] ERROR: short frame, fb->len=%u expected=%u\n",
                      (unsigned)fb->len, (unsigned)expected_bytes);
        return false;
    }

    // Center-crop to a square of side = min(w, h). For QVGA this is 240.
    int crop_size = (src_w < src_h) ? src_w : src_h;
    int crop_x0 = (src_w - crop_size) / 2;
    int crop_y0 = (src_h - crop_size) / 2;

    Serial.printf("[cam] preprocess src=%dx%d, crop=%dx%d@(%d,%d), dst=%dx%d\n",
                  src_w, src_h, crop_size, crop_size, crop_x0, crop_y0,
                  MODEL_INPUT_SIZE, MODEL_INPUT_SIZE);

    uint32_t t0 = millis();
    bilinear_crop_resize_quantize(
        fb->buf, src_w, src_h, crop_x0, crop_y0, crop_size,
        MODEL_INPUT_SIZE, out_buf);
    uint32_t dt = millis() - t0;
    Serial.printf("[cam] preprocess done in %u ms\n", (unsigned)dt);
    return true;
}
