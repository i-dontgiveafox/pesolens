// pesos firmware — TFLite Micro inference module
// Owns the tensor arena, model interpreter, YOLOv8 head decode, and NMS.

#pragma once

#include <stdint.h>
#include <vector>

struct Detection {
    uint8_t class_id;   // 0..MODEL_NUM_CLASSES-1
    float   conf;       // post-NMS confidence
    float   x;          // bbox center x in normalized [0,1]
    float   y;          // bbox center y
    float   w;          // bbox width
    float   h;          // bbox height
};

// One-time interpreter setup: allocate arena, parse model header, bind ops.
bool infer_init();

// Run the model on a preprocessed int8 input buffer
// (MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3 bytes, NHWC).
// Returns post-NMS detections filtered by CONF_THRESHOLD.
std::vector<Detection> infer_run(const int8_t* input);
