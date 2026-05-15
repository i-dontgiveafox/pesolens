// pesos firmware — audio module
// Owns I2S init for MAX98357A and SPIFFS-backed WAV playback.

#pragma once

#include <vector>

// One-time I2S + SPIFFS init.
bool audio_init();

// Play a single clip by its base filename (no extension).
// e.g. audio_play_clip("beep") → /spiffs/beep.wav
void audio_play_clip(const char* clip_id);

// Play a sequence of clips in order, with a small inter-clip gap.
void audio_play_sequence(const std::vector<const char*>& clip_ids);

// Tear down I2S + SPIFFS. Call before deep sleep so the amp clock stops
// and SPIFFS unmounts cleanly (lower idle current).
void audio_deinit();
