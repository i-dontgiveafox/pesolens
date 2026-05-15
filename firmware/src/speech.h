// pesos firmware — speech composer
// Maps an integer peso total to a sequence of English clip IDs.
// e.g. 731 → {"seven","hundred","thirty","one","pesos"}
// Clip IDs map 1:1 to filenames in firmware/data/<id>.wav.

#pragma once

#include <stdint.h>
#include <vector>

// Returns clip IDs in playback order. Returned pointers are static string
// literals; caller does not own them.
std::vector<const char*> compose_english(uint32_t pesos);
