// pesos firmware — main entry point
// Hardware: Seeed XIAO ESP32-S3 Sense + MAX98357A I2S amp + bone-conduction speaker
// Flow:
// - normal build: deep sleep -> button wake -> capture -> infer -> speak -> sleep
// - USB demo build: power on -> continuous capture/infer loop

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "config.h"
#include "camera.h"
#include "inference.h"
#include "counting.h"
#include "speech.h"
#include "audio.h"
#include "power.h"

static const char* g_shutdown_reason = "normal shutdown";

// Why the chip last restarted. Note: a reset via the EN/RST pin reports as
// POWERON on the ESP32-S3, so it is indistinguishable from a power cycle.
static const char* reset_reason_name(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON (power-up or EN/RST pin)";
        case ESP_RST_EXT:       return "EXT (external pin)";
        case ESP_RST_SW:        return "SW (software restart)";
        case ESP_RST_PANIC:     return "PANIC (firmware crash)";
        case ESP_RST_INT_WDT:   return "INT_WDT (interrupt watchdog)";
        case ESP_RST_TASK_WDT:  return "TASK_WDT (task watchdog)";
        case ESP_RST_WDT:       return "WDT (other watchdog)";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP (wake from deep sleep)";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (supply voltage dipped)";
        default:                return "UNKNOWN";
    }
}

// Crash, watchdog, or brownout: the previous run did not end normally.
static bool reset_was_abnormal(esp_reset_reason_t r) {
    return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
           r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
}

static void ready_led_init() {
#if READY_LED_PIN >= 0
    pinMode(READY_LED_PIN, OUTPUT);
    digitalWrite(READY_LED_PIN, READY_LED_OFF_LEVEL);
#endif
}

static void ready_led_set(bool ready, const char* reason = nullptr) {
#if READY_LED_PIN >= 0
    digitalWrite(READY_LED_PIN, ready ? READY_LED_ON_LEVEL : READY_LED_OFF_LEVEL);
    if (reason) {
        Serial.printf("[ready] D%u/GPIO%u LED %s - %s at %lu ms\n",
                      (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN,
                      ready ? "ON" : "OFF", reason, (unsigned long)millis());
    }
#else
    (void)ready;
    if (reason) {
        Serial.printf("[ready] no LED configured - %s at %lu ms\n",
                      reason, (unsigned long)millis());
    }
#endif
}

#ifdef PESOS_CONTINUOUS_SCAN
#ifndef PESOS_MANUAL_CAPTURE
static void auto_ready_window(uint32_t cycle) {
    ready_led_set(true, "auto ready window; place bill in view");
    Serial.printf("[cycle] #%u ready window start: %u ms before capture\n",
                  (unsigned)cycle, (unsigned)AUTO_SCAN_READY_MS);
    const uint32_t t_ready_start = millis();
    delay(AUTO_SCAN_READY_MS);
    Serial.printf("[cycle] #%u ready window done: elapsed=%lu ms; capture starts now\n",
                  (unsigned)cycle, (unsigned long)(millis() - t_ready_start));
    ready_led_set(false, "capture/process/audio busy");
}
#endif
#endif

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
    const uint32_t t_scan_start = millis();
    Serial.printf("[scan] capture start at %lu ms\n", (unsigned long)t_scan_start);
#ifdef PESOS_DUMP_FIRST_FRAME
    static bool dumped_debug_frame = false;
#endif

    camera_fb_t* fb = camera_capture();
    if (!fb) {
        Serial.println("[err] camera_capture returned null");
        return false;
    }
    const uint32_t t_cap = millis();
    Serial.printf("[scan] capture complete in %u ms; preprocessing frame\n",
                  (unsigned)(t_cap - t_scan_start));

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
    const uint32_t t_prep = millis();
    Serial.printf("[scan] preprocess complete in %u ms; inference start\n",
                  (unsigned)(t_prep - t_cap));

    std::vector<Detection> detections = infer_run(input_buf);
    const uint32_t t_inf = millis();
    Serial.printf("[scan] inference complete in %u ms; detections=%u\n",
                  (unsigned)(t_inf - t_prep), (unsigned)detections.size());
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

    // --- DEBUG: prominent result banner + per-phase timing ---
    // Multi-bill aware: report the bill count and total, plus a per-denomination
    // breakdown. Do NOT print a single class name for the whole scan — with
    // SINGLE_BILL_MODE off a frame can hold several bills of different values.
    if (!detections.empty() && total > 0) {
        if (detections.size() == 1) {
            const uint8_t cid = detections[0].class_id;
            const char* nm = cid < MODEL_NUM_CLASSES ? MODEL_CLASSES[cid].name : "?";
            Serial.printf(">>>>>>>>>>  DETECTED: 1 bill  P%s  =  P%u  <<<<<<<<<<\n",
                          nm, (unsigned)total);
        } else {
            Serial.printf(">>>>>>>>>>  DETECTED: %u bills  =  P%u  <<<<<<<<<<\n",
                          (unsigned)detections.size(), (unsigned)total);
            const std::map<uint8_t, uint8_t> by_class = count_by_class(detections);
            Serial.print("[result] breakdown:");
            for (const auto& kv : by_class) {
                const char* nm =
                    kv.first < MODEL_NUM_CLASSES ? MODEL_CLASSES[kv.first].name : "?";
                Serial.printf(" %ux P%s", (unsigned)kv.second, nm);
            }
            Serial.println();
        }
    } else {
        Serial.println(">>>>>>>>>>  NO BILL DETECTED / UNDETERMINED  <<<<<<<<<<");
    }
    Serial.printf("[time] capture=%u ms preprocess=%u ms infer=%u ms total=%u ms\n",
                  (unsigned)(t_cap - t_scan_start),
                  (unsigned)(t_prep - t_cap),
                  (unsigned)(t_inf - t_prep),
                  (unsigned)(millis() - t_scan_start));

#ifdef PESOS_MANUAL_CAPTURE
    ready_led_blink_before_audio();
#endif

    Serial.println("[audio-out] start - ready LED remains off");
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
    Serial.println("[audio-out] done");

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
#ifdef PESOS_CAPTURE_ONLY_DUMP
static void capture_dump_once(uint32_t cycle) {
    const uint32_t t_capture_start = millis();
    Serial.printf("\n[capture-only] #%u capture start at %lu ms\n",
                  (unsigned)cycle, (unsigned long)t_capture_start);

    camera_fb_t* fb = camera_capture();
    if (!fb) {
        Serial.println("[capture-only] ERROR: camera_capture returned null");
        return;
    }

    camera_dump_jpeg_to_serial(fb);
    camera_release(fb);

    Serial.printf("[capture-only] #%u done in %lu ms\n",
                  (unsigned)cycle, (unsigned long)(millis() - t_capture_start));
}
#else
static int8_t* g_input_buf = nullptr;
static bool g_audio_ready = false;
#endif
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
    const esp_reset_reason_t reset_reason = esp_reset_reason();
    Serial.printf("[boot] reset reason: %s\n", reset_reason_name(reset_reason));
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
    Serial.println("[mode] auto capture enabled - no D6 button required");
    Serial.printf("[mode] D%u/GPIO%u is ready LED only; capture runs after %u ms LED-on window\n",
                  (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN,
                  (unsigned)AUTO_SCAN_READY_MS);
#endif
#ifdef PESOS_MANUAL_CAPTURE
    pinMode(CAPTURE_BUTTON_PIN, INPUT_PULLUP);
    Serial.printf("[manual] D6/GPIO%u button enabled - press to start %u ms aim delay\n",
                  (unsigned)CAPTURE_BUTTON_PIN, (unsigned)MANUAL_CAPTURE_DELAY_MS);
#endif

    if (!camera_init()) {
        fail_and_sleep("camera_init failed");
    }

#ifdef PESOS_CAPTURE_ONLY_DUMP
    Serial.printf("[capture-only] auto JPEG dump mode armed: capture every %u ms\n",
                  (unsigned)AUTO_CAPTURE_INTERVAL_MS);
    ready_led_set(true, "capture-only mode ready");
    return;
#else
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
#ifndef PESOS_NO_BOOT_AUDIO_SELFTEST
    else {
        // Boot self-test: prove I2S -> MAX98357A -> speaker works in THIS build
        // before anything depends on a successful scan.
        //
        // Why this exists: the only evidence of working audio came from
        // env:audio_probe, which is framework=arduino and therefore puts the
        // console on USB CDC. Every *_tflm env is framework=arduino,espidf and
        // took CONFIG_ESP_CONSOLE_UART_DEFAULT, putting the console on UART0 =
        // GPIO43/44 — the same pads as CAPTURE_BUTTON_PIN and I2S_LRC. So the
        // probe could never speak for the full firmware. This beep runs with no
        // button press and no inference, so it isolates audio from the trigger:
        // silence here = init/pin-mux fault; sound here = audio is fine and the
        // fault is downstream in the scan path.
        //
        // Without a serial monitor, the beep pattern reports why the chip
        // restarted: 1 beep = normal power-up, 3 beeps = crash, watchdog, or
        // brownout.
        if (reset_was_abnormal(reset_reason)) {
            Serial.println("[audio] boot self-test: abnormal reset, playing 3 beeps");
            audio_play_sequence({"beep", "beep", "beep"});
        } else {
            Serial.println("[audio] boot self-test: playing beep");
            audio_play_clip("beep");
        }
        Serial.println("[audio] boot self-test done");
    }
#endif

#ifdef PESOS_MANUAL_CAPTURE
    Serial.println("[scan] manual mode - waiting for D6/GPIO43 press");
    ready_led_set(true);
    Serial.printf("[ready] D%u/GPIO%u LED on - ready for capture\n",
                  (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
#else
    Serial.printf("[scan] auto mode armed: LED turns on for %u ms before every capture\n",
                  (unsigned)AUTO_SCAN_READY_MS);
#endif
    return;
#endif
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
#ifdef PESOS_CAPTURE_ONLY_DUMP
    static uint32_t g_capture_cycle = 0;
    static uint32_t next_capture_ms = 0;

    const uint32_t now = millis();
    if ((int32_t)(now - next_capture_ms) >= 0) {
        next_capture_ms = now + AUTO_CAPTURE_INTERVAL_MS;
        ready_led_set(false, "capture-only busy");
        capture_dump_once(++g_capture_cycle);
        ready_led_set(true, "capture-only waiting for next interval");
    }
    delay(20);
#else
#ifdef PESOS_MANUAL_CAPTURE
    if (manual_capture_pressed()) {
        ready_led_set(false);
        Serial.printf("[ready] D%u/GPIO%u LED off - capture busy\n",
                      (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
        Serial.printf("[manual] D6 capture button pressed - capture in %u ms\n",
                      (unsigned)MANUAL_CAPTURE_DELAY_MS);
        // Acknowledge the press audibly so the user knows the scan started.
        // The beep's ~200 ms comes out of the aim delay, so capture timing is unchanged.
        const uint32_t t_press = millis();
        if (g_audio_ready) {
            audio_play_clip("beep");
        }
        const uint32_t ack_ms = millis() - t_press;
        if (ack_ms < MANUAL_CAPTURE_DELAY_MS) {
            delay(MANUAL_CAPTURE_DELAY_MS - ack_ms);
        }
        Serial.println("[manual] capture delay done");
        scan_once(g_input_buf, g_audio_ready);
        ready_led_set(true);
        Serial.printf("[ready] D%u/GPIO%u LED on - ready for next capture\n",
                      (unsigned)READY_LED_XIAO_D_PIN, (unsigned)READY_LED_PIN);
        Serial.println("[scan] manual mode - waiting for D6/GPIO43 press");
    }
    delay(10);
#else
    static uint32_t g_cycle = 0;
    const uint32_t cycle = ++g_cycle;
    const uint32_t t_cycle_start = millis();
    Serial.printf("\n[cycle] #%u start at %lu ms\n",
                  (unsigned)cycle, (unsigned long)t_cycle_start);
    auto_ready_window(cycle);
    const uint32_t t_busy_start = millis();
    bool detected = scan_once(g_input_buf, g_audio_ready);
    const uint32_t t_cycle_end = millis();
    Serial.printf("[cycle] #%u complete: detected=%s busy_ms=%lu total_cycle_ms=%lu\n",
                  (unsigned)cycle, detected ? "yes" : "no",
                  (unsigned long)(t_cycle_end - t_busy_start),
                  (unsigned long)(t_cycle_end - t_cycle_start));
#endif
#endif
#else
    // Never reached — shutdown_and_sleep() does not return.
#endif
}
