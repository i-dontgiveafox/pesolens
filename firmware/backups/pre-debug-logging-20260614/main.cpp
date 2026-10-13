// pesos firmware — main entry point
// Hardware: Seeed XIAO ESP32-S3 Sense + MAX98357A I2S amp + bone-conduction speaker
// Flow:
// - normal build: deep sleep -> button wake -> capture -> infer -> speak -> sleep
// - USB demo build: power on -> continuous capture/infer loop

#include <Arduino.h>
#include <esp_sleep.h>

#include "config.h"
#include "camera.h"
#include "inference.h"
#include "counting.h"
#include "speech.h"
#include "audio.h"
#include "power.h"

static const char* g_shutdown_reason = "normal shutdown";

static void ready_led_init() {
#if READY_LED_PIN >= 0
    pinMode(READY_LED_PIN, OUTPUT);
    digitalWrite(READY_LED_PIN, READY_LED_OFF_LEVEL);
#endif
}

static void ready_led_set(bool ready) {
#if READY_LED_PIN >= 0
    digitalWrite(READY_LED_PIN, ready ? READY_LED_ON_LEVEL : READY_LED_OFF_LEVEL);
#else
    (void)ready;
#endif
}

static void ready_led_blink_before_audio() {
#if READY_LED_PIN >= 0
    Serial.printf("[ready] D%u/GPIO%u LED blink %u times before audio\n",
                  (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN,
                  (unsigned)READY_LED_BLINK_COUNT);
    for (uint8_t i = 0; i < READY_LED_BLINK_COUNT; ++i) {
        digitalWrite(READY_LED_PIN, READY_LED_ON_LEVEL);
        delay(READY_LED_BLINK_MS);
        digitalWrite(READY_LED_PIN, READY_LED_OFF_LEVEL);
        delay(READY_LED_BLINK_MS);
    }
#endif
}

// Tear down peripherals that hold clocks / open file handles, then sleep.
// Camera state is lost on deep sleep so esp_camera_deinit isn't strictly
// required, but the I2S amp will keep clocking junk into the speaker if we
// don't stop it explicitly — call audio_deinit() before sleep.
static void shutdown_and_sleep() {
    ready_led_set(false);
    audio_deinit();
#if defined(PESOS_DEBUG_NO_SLEEP) || defined(PESOS_CONTINUOUS_SCAN)
    Serial.println("[debug] staying awake instead of deep sleep");
    Serial.printf("[debug] last shutdown reason: %s\n", g_shutdown_reason);
    Serial.flush();
    for (uint32_t i = 0;; ++i) {
        Serial.printf("[debug] alive %lu ms heartbeat=%lu reason=%s\n",
                      (unsigned long)millis(), (unsigned long)i, g_shutdown_reason);
        Serial.flush();
        delay(1000);
    }
#else
    power_enter_deep_sleep();   // does not return
#endif
}

// Play the failure clip if audio is up, then sleep.
static void fail_and_sleep(const char* tag) {
    g_shutdown_reason = tag;
    Serial.printf("[err] %s\n", tag);
    if (audio_init()) {
        audio_play_clip("error");
    }
    shutdown_and_sleep();
}

static void wait_for_serial_monitor() {
#ifdef PESOS_NO_SERIAL_WAIT
    delay(100);
#else
    uint32_t deadline = millis() + 5000;
    while (!Serial && millis() < deadline) {
        delay(10);
    }
    delay(500);
#endif
}

static void play_announcement(uint32_t total, bool audio_ready) {
    if (!audio_ready) {
        Serial.printf("[speak] skipped P%u announcement: audio not ready\n", (unsigned)total);
        return;
    }

    std::vector<const char*> words = compose_english(total);
    Serial.printf("[speak] announcing P%u with %u clips: ",
                  (unsigned)total, (unsigned)words.size());
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) Serial.print(' ');
        Serial.print(words[i] ? words[i] : "?");
    }
    Serial.println();

    audio_play_sequence(words);
    Serial.println("[speak] announcement done");
}

static bool scan_once(int8_t* input_buf, bool audio_ready) {
    Serial.println("[scan] capture start");
#ifdef PESOS_DUMP_FIRST_FRAME
    static bool dumped_debug_frame = false;
#endif

    camera_fb_t* fb = camera_capture();
    if (!fb) {
        Serial.println("[err] camera_capture returned null");
        return false;
    }

#ifdef PESOS_DUMP_FIRST_FRAME
    if (!dumped_debug_frame) {
        camera_dump_jpeg_to_serial(fb);
        dumped_debug_frame = true;
    }
#endif

    bool prep_ok = camera_preprocess_for_model(fb, input_buf);
    if (!prep_ok) {
        camera_release(fb);
        Serial.println("[err] camera_preprocess_for_model failed");
        return false;
    }

    std::vector<Detection> detections = infer_run(input_buf);
    for (size_t i = 0; i < detections.size(); ++i) {
        const Detection& d = detections[i];
        const uint16_t denom =
            d.class_id < MODEL_NUM_CLASSES
                ? MODEL_CLASSES[d.class_id].denomination
                : 0;
        const char* name =
            d.class_id < MODEL_NUM_CLASSES
                ? MODEL_CLASSES[d.class_id].name
                : "?";
        Serial.printf("[detect] #%u class=%s denom=P%u conf=%.3f box=(%.2f,%.2f,%.2f,%.2f)\n",
                      (unsigned)i + 1, name, (unsigned)denom, d.conf,
                      d.x, d.y, d.w, d.h);
    }

    uint32_t total = count_total_pesos(detections);
    Serial.printf("[result] %u pesos from %d bills\n",
                  (unsigned)total, (int)detections.size());

#ifdef PESOS_MANUAL_CAPTURE
    ready_led_blink_before_audio();
#endif

    if (total > 0) {
        play_announcement(total, audio_ready);
    } else {
        if (detections.empty()) {
            Serial.println("[speak] no bill detected; playing error tone");
        } else {
            Serial.println("[speak] detections produced P0; playing error tone");
        }
        if (audio_ready) {
            audio_play_clip("error");
        } else {
            Serial.println("[speak] skipped error tone: audio not ready");
        }
    }

#ifdef PESOS_MANUAL_CAPTURE
    if (audio_ready) {
        Serial.println("[ready] beep");
        audio_play_clip("beep");
    } else {
        Serial.println("[ready] audio not ready; skipped beep");
    }
#endif

#ifdef PESOS_DUMP_EACH_CAPTURE
    Serial.printf("[frame] dumping capture total=P%u bills=%u\n",
                  (unsigned)total, (unsigned)detections.size());
    camera_dump_jpeg_to_serial(fb);
#elif defined(PESOS_DUMP_DETECTED_FRAMES)
    if (!detections.empty()) {
        Serial.printf("[frame] dumping detected frame total=P%u bills=%u\n",
                      (unsigned)total, (unsigned)detections.size());
        camera_dump_jpeg_to_serial(fb);
    }
#endif
    camera_release(fb);

    return !detections.empty();
}

#ifdef PESOS_CONTINUOUS_SCAN
static int8_t* g_input_buf = nullptr;
static bool g_audio_ready = false;
#endif

#ifdef PESOS_MANUAL_CAPTURE
static bool manual_capture_pressed() {
    static bool armed = true;
    const bool pressed = digitalRead(CAPTURE_BUTTON_PIN) == LOW;

    if (!pressed) {
        armed = true;
        return false;
    }

    if (!armed) {
        return false;
    }

    delay(30);
    if (digitalRead(CAPTURE_BUTTON_PIN) != LOW) {
        return false;
    }

    armed = false;
    return true;
}
#endif

void setup() {
    Serial.begin(115200);
    wait_for_serial_monitor();
    Serial.println("\n=== pesos firmware boot ===");
    ready_led_init();

    if (!psram_check()) {
        // PSRAM is required for camera FB + model arena. Halting drains the
        // battery — sleep instead so the user can replace/retry the board.
        Serial.println("[fatal] PSRAM not available — sleeping");
        if (audio_init()) audio_play_clip("error");
        shutdown_and_sleep();
    }

    power_configure_wake_source();
    esp_sleep_wakeup_cause_t wake = power_get_wake_cause();
    power_print_wake_reason();

#ifdef PESOS_CONTINUOUS_SCAN
#ifdef PESOS_MANUAL_CAPTURE
    Serial.println("[mode] D6/GPIO43 manual capture enabled");
#else
    Serial.println("[mode] continuous scan enabled - no button required");
#endif
#ifdef PESOS_MANUAL_CAPTURE
    pinMode(CAPTURE_BUTTON_PIN, INPUT_PULLUP);
    Serial.printf("[manual] D6/GPIO%u button enabled - press to start %u ms aim delay\n",
                  (unsigned)CAPTURE_BUTTON_PIN, (unsigned)MANUAL_CAPTURE_DELAY_MS);
#endif

    if (!camera_init()) {
        fail_and_sleep("camera_init failed");
    }

    g_input_buf = (int8_t*)heap_caps_malloc(
        MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_input_buf) {
        fail_and_sleep("PSRAM alloc failed for input buffer");
    }

    if (!infer_init()) {
        fail_and_sleep("infer_init failed - see prior log line for cause");
    }

    g_audio_ready = audio_init();
    if (!g_audio_ready) {
        Serial.println("[warn] audio_init failed; serial detection will continue without playback");
    }

#ifdef PESOS_MANUAL_CAPTURE
    Serial.println("[scan] manual mode - waiting for D6/GPIO43 press");
    ready_led_set(true);
    Serial.printf("[ready] D%u/GPIO%u LED on - ready for capture\n",
                  (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
#else
    ready_led_set(true);
    Serial.printf("[scan] ready - point a bill at the camera; scanning every %u ms\n",
                  (unsigned)AUTO_SCAN_INTERVAL_MS);
    Serial.printf("[ready] D%u/GPIO%u LED on - ready for auto scan\n",
                  (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
#endif
    return;
#else
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

    // Model input buffer — allocate from PSRAM, not DRAM (DRAM is too tight).
    int8_t* input_buf = (int8_t*)heap_caps_malloc(
        MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!input_buf) {
        camera_release(fb);
        fail_and_sleep("PSRAM alloc failed for input buffer");
    }
    bool prep_ok = camera_preprocess_for_model(fb, input_buf);
    if (!prep_ok) {
        camera_release(fb);
        heap_caps_free(input_buf);
        fail_and_sleep("camera_preprocess_for_model failed");
    }

    if (!infer_init()) {
        // infer_init logs the specific cause (no model bytes, arena alloc fail,
        // schema mismatch, etc.). Skip inference and play the error clip.
        camera_release(fb);
        heap_caps_free(input_buf);
        fail_and_sleep("infer_init failed — see prior log line for cause");
    }

    std::vector<Detection> detections = infer_run(input_buf);
    uint32_t total = count_total_pesos(detections);
    Serial.printf("[result] %u pesos from %d bills\n",
                  (unsigned)total, (int)detections.size());

#ifdef PESOS_DUMP_DETECTED_FRAMES
    if (!detections.empty()) {
        Serial.printf("[frame] dumping detected frame total=P%u bills=%u\n",
                      (unsigned)total, (unsigned)detections.size());
        camera_dump_jpeg_to_serial(fb);
    }
#endif
    camera_release(fb);

    if (!audio_init()) {
        Serial.println("[warn] audio_init failed; skipping playback");
    } else {
        play_announcement(total, true);
    }

    heap_caps_free(input_buf);
    shutdown_and_sleep();
#endif
}

void loop() {
#ifdef PESOS_CONTINUOUS_SCAN
#ifdef PESOS_MANUAL_CAPTURE
    if (manual_capture_pressed()) {
        ready_led_set(false);
        Serial.printf("[ready] D%u/GPIO%u LED off - capture busy\n",
                      (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
        Serial.printf("[manual] D6 capture button pressed - capture in %u ms\n",
                      (unsigned)MANUAL_CAPTURE_DELAY_MS);
        delay(MANUAL_CAPTURE_DELAY_MS);
        Serial.println("[manual] capture delay done");
        scan_once(g_input_buf, g_audio_ready);
        ready_led_set(true);
        Serial.printf("[ready] D%u/GPIO%u LED on - ready for next capture\n",
                      (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
        Serial.println("[scan] manual mode - waiting for D6/GPIO43 press");
    }
    delay(10);
#else
    ready_led_set(false);
    Serial.printf("[ready] D%u/GPIO%u LED off - scan busy\n",
                  (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
    bool detected = scan_once(g_input_buf, g_audio_ready);
    ready_led_set(true);
    Serial.printf("[ready] D%u/GPIO%u LED on - ready for next auto scan\n",
                  (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
    delay(detected ? AUTO_SCAN_AFTER_DETECT_MS : AUTO_SCAN_INTERVAL_MS);
#endif
#else
    // Never reached — shutdown_and_sleep() does not return.
#endif
}
