// pesos firmware — counting module
// Pure post-detection arithmetic: detections → peso totals.

#pragma once

#include <stdint.h>
#include <vector>
#include <map>
#include "inference.h"

// Sum every detection's denomination, return total pesos.
uint32_t count_total_pesos(const std::vector<Detection>& detections);

// Bucket by class_id → count, for richer announcement modes.
std::map<uint8_t, uint8_t> count_by_class(const std::vector<Detection>& detections);
