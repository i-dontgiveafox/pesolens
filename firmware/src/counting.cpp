// pesos firmware — counting module
// Pure post-detection arithmetic: detections → peso totals.

#include <Arduino.h>
#include "counting.h"
#include "config.h"

uint32_t count_total_pesos(const std::vector<Detection>& detections) {
    uint32_t total = 0;
    for (const auto& d : detections) {
        if (d.class_id < MODEL_NUM_CLASSES) {
            total += DENOMINATIONS[d.class_id];
        }
    }
    Serial.printf("[count] %u bills -> P%u\n",
                  (unsigned)detections.size(), (unsigned)total);
    return total;
}

std::map<uint8_t, uint8_t> count_by_class(const std::vector<Detection>& detections) {
    std::map<uint8_t, uint8_t> result;
    for (const auto& d : detections) {
        if (d.class_id < MODEL_NUM_CLASSES) {
            result[d.class_id]++;
        }
    }
    return result;
}
