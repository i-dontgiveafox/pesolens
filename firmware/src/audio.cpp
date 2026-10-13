// pesos firmware — audio module
// I2S TX → MAX98357A → bone-conduction speaker, streaming WAVs from SPIFFS.
// Format expected: 16 kHz, 16-bit PCM, mono.

#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <driver/i2s.h>

#include "audio.h"
#include "config.h"

namespace {

constexpr i2s_port_t I2S_PORT       = I2S_NUM_0;
constexpr uint32_t   SAMPLE_RATE_HZ = 16000;
constexpr size_t     STREAM_BUF_SZ  = 1024;
constexpr uint32_t   INTER_CLIP_MS  = 60;
constexpr size_t     STEREO_BUF_SZ  = STREAM_BUF_SZ * 2;

bool g_audio_ready = false;

// Locate the "data" chunk in a RIFF/WAVE file and leave the file positioned at the
// first PCM sample. Returns the data chunk byte length (0 on failure).
uint32_t seek_to_wav_data(File& f) {
    uint8_t hdr[12];
    if (f.read(hdr, 12) != 12) return 0;
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        return 0;
    }

    while (f.available() >= 8) {
        uint8_t chunk[8];
        if (f.read(chunk, 8) != 8) return 0;
        uint32_t size = (uint32_t)chunk[4]
                      | ((uint32_t)chunk[5] << 8)
                      | ((uint32_t)chunk[6] << 16)
                      | ((uint32_t)chunk[7] << 24);
        if (memcmp(chunk, "data", 4) == 0) {
            return size;
        }
        if (!f.seek(f.position() + size)) return 0;
    }
    return 0;
}

void write_silence(uint32_t ms) {
    if (!g_audio_ready || ms == 0) return;

    static uint8_t zeros[STEREO_BUF_SZ];
    memset(zeros, 0, sizeof(zeros));

    // 16-bit stereo @ 16 kHz = 64000 bytes/sec.
    uint32_t total_bytes = (SAMPLE_RATE_HZ * 2 * 2 * ms) / 1000;
    size_t written;
    while (total_bytes > 0) {
        size_t chunk = total_bytes > sizeof(zeros) ? sizeof(zeros) : total_bytes;
        i2s_write(I2S_PORT, zeros, chunk, &written, portMAX_DELAY);
        total_bytes -= chunk;
    }
}

void write_mono_pcm_as_stereo(const uint8_t* mono, size_t bytes) {
    static uint8_t stereo[STEREO_BUF_SZ];
    size_t offset = 0;

    while (offset + 1 < bytes) {
        size_t samples = (bytes - offset) / 2;
        size_t max_samples = sizeof(stereo) / 4;
        if (samples > max_samples) {
            samples = max_samples;
        }

        for (size_t i = 0; i < samples; ++i) {
            const uint8_t lo = mono[offset + i * 2];
            const uint8_t hi = mono[offset + i * 2 + 1];
            stereo[i * 4] = lo;
            stereo[i * 4 + 1] = hi;
            stereo[i * 4 + 2] = lo;
            stereo[i * 4 + 3] = hi;
        }

        size_t written = 0;
        i2s_write(I2S_PORT, stereo, samples * 4, &written, portMAX_DELAY);
        offset += samples * 2;
    }
}

} // namespace

bool audio_init() {
    if (g_audio_ready) return true;

    // Note: pass `false` so a mount failure does NOT auto-format the partition.
    // Auto-format would silently erase all uploaded audio clips, breaking the
    // device on first failed mount. Caller fails loudly instead.
    if (!SPIFFS.begin(false)) {
        Serial.println("[audio] SPIFFS mount failed — run `pio run --target uploadfs` to load clips");
        return false;
    }

    i2s_config_t i2s_config = {};
    i2s_config.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate          = SAMPLE_RATE_HZ;
    i2s_config.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count        = 8;
    i2s_config.dma_buf_len          = 256;
    i2s_config.use_apll             = false;
    i2s_config.tx_desc_auto_clear   = true;

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[audio] i2s_driver_install failed: %d\n", err);
        SPIFFS.end();
        return false;
    }

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num   = I2S_BCLK;
    pin_config.ws_io_num    = I2S_LRC;
    pin_config.data_out_num = I2S_DIN;
    pin_config.data_in_num  = I2S_PIN_NO_CHANGE;
    pin_config.mck_io_num   = I2S_PIN_NO_CHANGE;

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[audio] i2s_set_pin failed: %d\n", err);
        i2s_driver_uninstall(I2S_PORT);
        SPIFFS.end();
        return false;
    }

    i2s_zero_dma_buffer(I2S_PORT);
    g_audio_ready = true;
    Serial.println("[audio] init ok (16 kHz, 16-bit, mono)");
    return true;
}

void audio_play_clip(const char* clip_id) {
    if (!g_audio_ready) {
        Serial.println("[audio] not initialized; skipping clip");
        return;
    }
    if (!clip_id || !*clip_id) {
        Serial.println("[audio] empty clip_id");
        return;
    }

    String path = "/";
    path += clip_id;
    path += ".wav";

    File f = SPIFFS.open(path.c_str(), "r");
    if (!f) {
        Serial.printf("[audio] missing clip: %s\n", path.c_str());
        return;
    }

    uint32_t data_bytes = seek_to_wav_data(f);
    if (data_bytes == 0) {
        // Fallback: assume vanilla 44-byte header.
        f.seek(44);
        data_bytes = f.size() > 44 ? f.size() - 44 : 0;
        Serial.printf("[audio] %s: no 'data' chunk, falling back to 44-byte skip\n", clip_id);
    }

    uint32_t t0 = millis();
    uint8_t buf[STREAM_BUF_SZ];
    size_t bytes_written;
    uint32_t streamed = 0;

    while (data_bytes > 0) {
        size_t want = data_bytes > sizeof(buf) ? sizeof(buf) : data_bytes;
        int n = f.read(buf, want);
        if (n <= 0) break;
        write_mono_pcm_as_stereo(buf, (size_t)n);
        bytes_written = (size_t)n;
        streamed += bytes_written;
        data_bytes -= (uint32_t)n;
    }
    f.close();

    uint32_t ms = millis() - t0;
    Serial.printf("[audio] playing %s (%ums)\n", clip_id, (unsigned)ms);
}

void audio_play_sequence(const std::vector<const char*>& clip_ids) {
    if (!g_audio_ready) {
        Serial.println("[audio] not initialized; skipping sequence");
        return;
    }

    for (size_t i = 0; i < clip_ids.size(); ++i) {
        const char* id = clip_ids[i];
        if (!id) continue;
        audio_play_clip(id);
        if (i + 1 < clip_ids.size()) {
            write_silence(INTER_CLIP_MS);
        }
    }
    // Trailing pad so the final word doesn't get cut off mechanically.
    write_silence(INTER_CLIP_MS);
}

void audio_deinit() {
    if (!g_audio_ready) return;
    // Flush any pending DMA so the last word/beep isn't truncated when the
    // caller (main.cpp) goes straight into deep sleep after this.
    write_silence(INTER_CLIP_MS);
    i2s_zero_dma_buffer(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
    SPIFFS.end();
    g_audio_ready = false;
    Serial.println("[audio] deinit");
}
