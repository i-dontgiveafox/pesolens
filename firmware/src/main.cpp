// pesos firmware — main entry point
// Hardware: Seeed XIAO ESP32-S3 Sense + MAX98357A I2S amp + bone-conduction speaker
// Flow: deep sleep → button wake → capture → infer → count → speak → sleep

#include <Arduino.h>
#include <esp_sleep.h>

#include "config.h"
#include "camera.h"
#include "inference.h"
#include "counting.h"
#include "speech.h"
#include "audio.h"
#include "power.h"

// Tear down peripherals that hold clocks / open file handles, then sleep.
// Camera state is lost on deep sleep so esp_camera_deinit isn't strictly
// required, but the I2S amp will keep clocking junk into the speaker if we
// don't stop it explicitly — call audio_deinit() before sleep.
static void shutdown_and_sleep() {
    audio_deinit();
    power_enter_deep_sleep();   // does not return
}

// Play the failure clip if audio is up, then sleep.
static void fail_and_sleep(const char* tag) {
    Serial.printf("[err] %s\n", tag);
    if (audio_init()) {
        audio_play_clip("error");
    }
    shutdown_and_sleep();
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== pesos firmware boot ===");

    if (!psram_check()) {
        // PSRAM is required for camera FB + model arena. Halting drains the
        // battery — sleep instead so the user can replace/retry the board.
        Serial.println("[fatal] PSRAM not available — sleeping");
        if (audio_init()) audio_play_clip("error");
        shutdown_and_sleep();
    }

    power_configure_wake_source();
    esp_sleep_wakeup_cause_t wake = power_get_wake_cause();

    if (wake != ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("[boot] first boot / non-button wake — confirming and sleeping");
        if (audio_init()) {
            audio_play_clip("beep");
        }
        shutdown_and_sleep();
    }

    // Wait for the user to release the button before proceeding. EXT0 is
    // level-triggered (active-low), so a held button would re-wake the chip
    // immediately after sleep. Debounce: require button to read HIGH for
    // 30 ms before continuing. Cap the wait at 5 s to avoid infinite hang.
    {
        uint32_t deadline = millis() + 5000;
        uint32_t stable_since = 0;
        while (millis() < deadline) {
            if (digitalRead(BUTTON_PIN) == HIGH) {
                if (stable_since == 0) stable_since = millis();
                if (millis() - stable_since >= 30) break;
            } else {
                stable_since = 0;
            }
            delay(2);
        }
    }

    Serial.println("[wake] button → capture");

    if (!camera_init()) {
        fail_and_sleep("camera_init failed");
    }

    camera_fb_t* fb = camera_capture();
    if (!fb) {
        fail_and_sleep("camera_capture returned null");
    }

    // 307 KB int8 buffer — allocate from PSRAM, not DRAM (DRAM is too tight).
    int8_t* input_buf = (int8_t*)heap_caps_malloc(
        MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!input_buf) {
        camera_release(fb);
        fail_and_sleep("PSRAM alloc failed for input buffer");
    }
    bool prep_ok = camera_preprocess_for_model(fb, input_buf);
    camera_release(fb);
    if (!prep_ok) {
        heap_caps_free(input_buf);
        fail_and_sleep("camera_preprocess_for_model failed");
    }

    if (!infer_init()) {
        // infer_init logs the specific cause (no model bytes, arena alloc fail,
        // schema mismatch, etc.). Skip inference and play the error clip.
        fail_and_sleep("infer_init failed — see prior log line for cause");
    }

    std::vector<Detection> detections = infer_run(input_buf);
    uint32_t total = count_total_pesos(detections);
    Serial.printf("[result] %u pesos from %d bills\n",
                  (unsigned)total, (int)detections.size());

    if (!audio_init()) {
        Serial.println("[warn] audio_init failed; skipping playback");
    } else {
        std::vector<const char*> words = compose_english(total);
        audio_play_sequence(words);
        audio_play_clip("beep");
    }

    heap_caps_free(input_buf);
    shutdown_and_sleep();
}

void loop() {
    // Never reached — shutdown_and_sleep() does not return.
}
