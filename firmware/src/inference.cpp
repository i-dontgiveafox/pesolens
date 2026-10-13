// pesos firmware — TFLite Micro inference module.
// Loads the embedded YOLOv8n int8 model from model_data.cpp, runs inference
// on a preprocessed MODEL_INPUT_SIZE x MODEL_INPUT_SIZE x 3 int8 NHWC input,
// and returns post-NMS detections.
//
// Output tensor shape from Ultralytics int8 export is [1, 4 + num_classes, anchors].
// Channel layout:
//   ch[0..3] = box (cx, cy, w, h), usually normalized 0..1 in TFLite exports
//   ch[4..]  = per-class confidences
// We also handle the transposed [1, anchors, 8] form just in case the export
// pipeline differs.

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "inference.h"
#include "../include/config.h"
#include "../include/model_data.h"

// =============================================================================
// TFLite Micro integration is gated behind ENABLE_TFLITE_MICRO.
// Default OFF because esp-tflite-micro is an ESP-IDF component, not a registered
// PlatformIO Arduino library. To enable real inference, switch this firmware to
// `framework = arduino, espidf` mixed mode and add esp-tflite-micro via the
// ESP-IDF Component Manager. See docs/SETUP_GUIDE.md "Known build issues".
// While disabled, infer_init() returns false and infer_run() returns no
// detections — main.cpp's failure path plays the "error" clip and sleeps.
// =============================================================================
#ifndef ENABLE_TFLITE_MICRO
#define ENABLE_TFLITE_MICRO 0
#endif

#if ENABLE_TFLITE_MICRO
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_log.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/schema/schema_generated.h>
#endif

#if ENABLE_TFLITE_MICRO

namespace {

// PSRAM-backed tensor arena. Allocated once in infer_init().
uint8_t* g_tensor_arena = nullptr;

const tflite::Model*       g_model        = nullptr;
tflite::MicroInterpreter*  g_interpreter  = nullptr;
TfLiteTensor*              g_input_tensor = nullptr;
TfLiteTensor*              g_output_tensor= nullptr;

// The Espressif esp-tflite-micro component does not provide AllOpsResolver, so
// keep an explicit resolver broad enough for the current Ultralytics YOLO export.
tflite::MicroMutableOpResolver<28> g_resolver;
bool g_ops_registered = false;

// Storage for the interpreter must outlive infer_init(). Placement-new into
// this byte buffer (TFLM lacks a default constructor).
alignas(tflite::MicroInterpreter) uint8_t g_interpreter_buf[sizeof(tflite::MicroInterpreter)];

// Detected output layout. Set during infer_init() by inspecting the output
// tensor's shape. Channels = 4 + MODEL_NUM_CLASSES; anchors depends on stride.
struct OutputLayout {
    int channels;       // expected = 4 + MODEL_NUM_CLASSES
    int anchors;        // depends on input size and YOLO strides
    bool channels_first;// true: [1, C, A]; false: [1, A, C]
    float scale;        // dequant params
    int   zero_point;
} g_out;

bool register_ops() {
    if (g_ops_registered) {
        return true;
    }

    TfLiteStatus status = kTfLiteOk;
    status = g_resolver.AddAdd();                    if (status != kTfLiteOk) return false;
    status = g_resolver.AddConcatenation();          if (status != kTfLiteOk) return false;
    status = g_resolver.AddConv2D();                 if (status != kTfLiteOk) return false;
    status = g_resolver.AddDepthwiseConv2D();        if (status != kTfLiteOk) return false;
    status = g_resolver.AddDequantize();             if (status != kTfLiteOk) return false;
    status = g_resolver.AddFullyConnected();         if (status != kTfLiteOk) return false;
    status = g_resolver.AddHardSwish();              if (status != kTfLiteOk) return false;
    status = g_resolver.AddLeakyRelu();              if (status != kTfLiteOk) return false;
    status = g_resolver.AddLogistic();               if (status != kTfLiteOk) return false;
    status = g_resolver.AddMaxPool2D();              if (status != kTfLiteOk) return false;
    status = g_resolver.AddMean();                   if (status != kTfLiteOk) return false;
    status = g_resolver.AddMul();                    if (status != kTfLiteOk) return false;
    status = g_resolver.AddPad();                    if (status != kTfLiteOk) return false;
    status = g_resolver.AddQuantize();               if (status != kTfLiteOk) return false;
    status = g_resolver.AddReshape();                if (status != kTfLiteOk) return false;
    status = g_resolver.AddResizeBilinear();         if (status != kTfLiteOk) return false;
    status = g_resolver.AddResizeNearestNeighbor();  if (status != kTfLiteOk) return false;
    status = g_resolver.AddShape();                  if (status != kTfLiteOk) return false;
    status = g_resolver.AddSlice();                  if (status != kTfLiteOk) return false;
    status = g_resolver.AddSoftmax();                if (status != kTfLiteOk) return false;
    status = g_resolver.AddSplit();                  if (status != kTfLiteOk) return false;
    status = g_resolver.AddSplitV();                 if (status != kTfLiteOk) return false;
    status = g_resolver.AddSqueeze();                if (status != kTfLiteOk) return false;
    status = g_resolver.AddStridedSlice();           if (status != kTfLiteOk) return false;
    status = g_resolver.AddSub();                    if (status != kTfLiteOk) return false;
    status = g_resolver.AddTranspose();              if (status != kTfLiteOk) return false;
    status = g_resolver.AddUnpack();                 if (status != kTfLiteOk) return false;

    g_ops_registered = true;
    return true;
}

// IoU for axis-aligned bboxes (cx,cy,w,h form).
float iou_xywh(float ax, float ay, float aw, float ah,
               float bx, float by, float bw, float bh) {
    const float ax1 = ax - aw * 0.5f, ay1 = ay - ah * 0.5f;
    const float ax2 = ax + aw * 0.5f, ay2 = ay + ah * 0.5f;
    const float bx1 = bx - bw * 0.5f, by1 = by - bh * 0.5f;
    const float bx2 = bx + bw * 0.5f, by2 = by + bh * 0.5f;

    const float ix1 = std::max(ax1, bx1);
    const float iy1 = std::max(ay1, by1);
    const float ix2 = std::min(ax2, bx2);
    const float iy2 = std::min(ay2, by2);
    const float iw = std::max(0.0f, ix2 - ix1);
    const float ih = std::max(0.0f, iy2 - iy1);
    const float inter = iw * ih;
    const float uni = aw * ah + bw * bh - inter;
    return uni > 0.0f ? inter / uni : 0.0f;
}

// NMS. Detections are sorted descending by confidence. With CLASS_AGNOSTIC_NMS
// enabled, overlapping boxes across classes suppress each other so one bill
// cannot be counted as several denominations.
std::vector<Detection> nms(std::vector<Detection> dets, float iou_thresh) {
    std::sort(dets.begin(), dets.end(),
              [](const Detection& a, const Detection& b) { return a.conf > b.conf; });
    std::vector<Detection> kept;
    std::vector<bool> suppressed(dets.size(), false);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (suppressed[i]) continue;
        kept.push_back(dets[i]);
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (suppressed[j]) continue;
#if !CLASS_AGNOSTIC_NMS
            if (dets[j].class_id != dets[i].class_id) continue;
#endif
            if (iou_xywh(dets[i].x, dets[i].y, dets[i].w, dets[i].h,
                         dets[j].x, dets[j].y, dets[j].w, dets[j].h) > iou_thresh) {
                suppressed[j] = true;
            }
        }
    }
    return kept;
}

inline float dequant(int8_t raw) {
    return (static_cast<int>(raw) - g_out.zero_point) * g_out.scale;
}

}  // namespace

bool infer_init() {
    if (tflite_model_len == 0) {
        Serial.println("[infer] ERROR: model not yet converted — run tools/convert_model.py");
        return false;
    }

    if (!g_tensor_arena) {
        g_tensor_arena = (uint8_t*)heap_caps_malloc(
            TENSOR_ARENA_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!g_tensor_arena) {
            Serial.println("[infer] ERROR: failed to allocate tensor arena in PSRAM");
            return false;
        }
    }

    g_model = tflite::GetModel(tflite_model);
    if (!g_model) {
        Serial.println("[infer] ERROR: GetModel returned null");
        return false;
    }
    if (g_model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[infer] ERROR: schema version mismatch %d vs %d\n",
                      (int)g_model->version(), (int)TFLITE_SCHEMA_VERSION);
        return false;
    }

    if (!register_ops()) {
        Serial.println("[infer] ERROR: failed to register TFLite Micro ops");
        return false;
    }

    g_interpreter = new (g_interpreter_buf) tflite::MicroInterpreter(
        g_model, g_resolver, g_tensor_arena, TENSOR_ARENA_SIZE);

    if (g_interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("[infer] ERROR: AllocateTensors failed");
        return false;
    }

    g_input_tensor  = g_interpreter->input(0);
    g_output_tensor = g_interpreter->output(0);

    // Validate input: [1, MODEL_INPUT_SIZE, MODEL_INPUT_SIZE, 3] int8.
    if (g_input_tensor->type != kTfLiteInt8) {
        Serial.printf("[infer] ERROR: expected int8 input, got type=%d\n",
                      (int)g_input_tensor->type);
        return false;
    }
    if (g_input_tensor->dims->size != 4 ||
        g_input_tensor->dims->data[0] != 1 ||
        g_input_tensor->dims->data[1] != MODEL_INPUT_SIZE ||
        g_input_tensor->dims->data[2] != MODEL_INPUT_SIZE ||
        g_input_tensor->dims->data[3] != 3) {
        Serial.printf("[infer] ERROR: unexpected input shape [%d,%d,%d,%d]\n",
                      g_input_tensor->dims->data[0], g_input_tensor->dims->data[1],
                      g_input_tensor->dims->data[2], g_input_tensor->dims->data[3]);
        return false;
    }
    const float expected_input_scale = 1.0f / 255.0f;
    if (std::fabs(g_input_tensor->params.scale - expected_input_scale) > 0.000001f ||
        g_input_tensor->params.zero_point != -128) {
        Serial.printf(
            "[infer] ERROR: input quantization scale=%.9f zero=%d, "
            "firmware requires scale=1/255 zero=-128\n",
            (double)g_input_tensor->params.scale,
            g_input_tensor->params.zero_point);
        return false;
    }

    // Validate output: [1, 4 + classes, anchors] or [1, anchors, 4 + classes] int8.
    if (g_output_tensor->type != kTfLiteInt8) {
        Serial.printf("[infer] ERROR: expected int8 output, got type=%d\n",
                      (int)g_output_tensor->type);
        return false;
    }
    if (g_output_tensor->dims->size != 3 || g_output_tensor->dims->data[0] != 1) {
        Serial.println("[infer] ERROR: output rank/batch unexpected");
        return false;
    }
    const int d1 = g_output_tensor->dims->data[1];
    const int d2 = g_output_tensor->dims->data[2];
    const int expected_ch = 4 + MODEL_NUM_CLASSES;
    if (d1 == expected_ch) {
        g_out.channels_first = true;
        g_out.channels = d1;
        g_out.anchors  = d2;
    } else if (d2 == expected_ch) {
        g_out.channels_first = false;
        g_out.channels = d2;
        g_out.anchors  = d1;
    } else {
        Serial.printf("[infer] ERROR: cannot map output shape [1,%d,%d] to %d channels\n",
                      d1, d2, expected_ch);
        return false;
    }
    g_out.scale      = g_output_tensor->params.scale;
    g_out.zero_point = g_output_tensor->params.zero_point;

    Serial.printf("[infer] ready — input [1,%d,%d,3] int8, output [1,%d,%d] %s, "
                  "arena=%u bytes PSRAM, used=%u\n",
                  MODEL_INPUT_SIZE, MODEL_INPUT_SIZE, d1, d2,
                  g_out.channels_first ? "(C,A)" : "(A,C)",
                  (unsigned)TENSOR_ARENA_SIZE,
                  (unsigned)g_interpreter->arena_used_bytes());
    Serial.print("[infer] class map");
    for (int c = 0; c < MODEL_NUM_CLASSES; ++c) {
        Serial.printf(" %d=P%u", c, (unsigned)MODEL_CLASSES[c].denomination);
    }
    Serial.println();
    return true;
}

std::vector<Detection> infer_run(const int8_t* input) {
    std::vector<Detection> out;
    if (!g_interpreter || !g_input_tensor || !g_output_tensor) {
        Serial.println("[infer] ERROR: infer_run before successful infer_init");
        return out;
    }

    const size_t n_in = MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3;
    std::memcpy(g_input_tensor->data.int8, input, n_in);

    const uint32_t invoke_start = millis();
    if (g_interpreter->Invoke() != kTfLiteOk) {
        Serial.println("[infer] ERROR: Invoke failed");
        return out;
    }
    const uint32_t invoke_ms = millis() - invoke_start;

    const int8_t* raw = g_output_tensor->data.int8;
    const int A = g_out.anchors;
    const int C = g_out.channels;          // 4 + MODEL_NUM_CLASSES
    const float inv_size = 1.0f / static_cast<float>(MODEL_INPUT_SIZE);

    out.reserve(64);

    struct CandidateLog {
        float score;
        int cls;
        float cx;
        float cy;
        float w;
        float h;
    };
    CandidateLog top[3] = {
        {-1.0f, 0, 0, 0, 0, 0},
        {-1.0f, 0, 0, 0, 0, 0},
        {-1.0f, 0, 0, 0, 0, 0},
    };
    uint16_t above_threshold = 0;
    uint16_t above_010 = 0, above_015 = 0, above_020 = 0;
    float global_best = -1.0f;
    float class_max[MODEL_NUM_CLASSES];
    for (int c = 0; c < MODEL_NUM_CLASSES; ++c) {
        class_max[c] = -1.0f;
    }

    auto remember_candidate = [&](float score, int cls, float cx, float cy, float w, float h) {
        for (int i = 0; i < 3; ++i) {
            if (score <= top[i].score) {
                continue;
            }
            for (int j = 2; j > i; --j) {
                top[j] = top[j - 1];
            }
            top[i] = {score, cls, cx, cy, w, h};
            break;
        }
    };

    // Helper to read element [channel c, anchor a] respecting layout.
    auto at = [&](int c, int a) -> float {
        const int idx = g_out.channels_first ? (c * A + a) : (a * C + c);
        return dequant(raw[idx]);
    };

    for (int a = 0; a < A; ++a) {
        // Find argmax class for this anchor.
        int best_cls = 0;
        float best_score = at(4, a);
        class_max[0] = std::max(class_max[0], best_score);
        for (int c = 1; c < MODEL_NUM_CLASSES; ++c) {
            const float s = at(4 + c, a);
            class_max[c] = std::max(class_max[c], s);
            if (s > best_score) { best_score = s; best_cls = c; }
        }

        if (best_score > global_best) global_best = best_score;
        if (best_score >= 0.10f) ++above_010;
        if (best_score >= 0.15f) ++above_015;
        if (best_score >= 0.20f) ++above_020;

        // Ultralytics TFLite exports commonly return normalized xywh, while
        // some variants return input-pixel xywh. Normalize either form for
        // logging and NMS.
        const float cx = at(0, a);
        const float cy = at(1, a);
        const float w  = at(2, a);
        const float h  = at(3, a);
        remember_candidate(best_score, best_cls, cx, cy, w, h);
        if (best_score < CONF_THRESHOLD) continue;
        if (w <= 0.0f || h <= 0.0f) continue;
        ++above_threshold;

        Detection d;
        const bool box_is_normalized =
            cx >= -0.5f && cx <= 1.5f &&
            cy >= -0.5f && cy <= 1.5f &&
            w  >=  0.0f && w  <= 2.0f &&
            h  >=  0.0f && h  <= 2.0f;
        const float box_scale = box_is_normalized ? 1.0f : inv_size;

        d.class_id = static_cast<uint8_t>(best_cls);
        d.conf     = best_score;
        d.x = cx * box_scale;
        d.y = cy * box_scale;
        d.w = w  * box_scale;
        d.h = h  * box_scale;
        out.push_back(d);
    }

    std::vector<Detection> kept = nms(std::move(out), IOU_THRESHOLD);
    const size_t nms_detections = kept.size();
#if SINGLE_BILL_MODE
    if (kept.size() > 1) {
        Serial.printf(
            "[infer] single-bill mode: keeping highest-confidence detection "
            "and discarding %u extra boxes\n",
            (unsigned)(kept.size() - 1));
        kept.resize(1);
    }
#endif
    Serial.printf(
        "[infer] invoke=%u ms, threshold=%.2f, candidates>threshold=%u, "
        "nms_detections=%u, returned=%u\n",
                  (unsigned)invoke_ms, (double)CONF_THRESHOLD,
                  (unsigned)above_threshold, (unsigned)nms_detections,
                  (unsigned)kept.size());
    Serial.print("[infer] class_max");
    for (int c = 0; c < MODEL_NUM_CLASSES; ++c) {
        Serial.printf(" %s=%.3f", MODEL_CLASSES[c].name, class_max[c]);
    }
    Serial.println();
    // --- DEBUG: confidence distribution + output dequant params ---
    // global_best is the single highest class score anywhere in the frame.
    // If it stays ~0.02 on a clear bill, the model itself is blind to it.
    Serial.printf("[infer] conf-dist anchors=%d global_best=%.3f  >=0.10:%u  >=0.15:%u  "
                  ">=0.20:%u  >=%.2f:%u   out_quant scale=%.6f zero=%d\n",
                  A, (double)global_best,
                  (unsigned)above_010, (unsigned)above_015, (unsigned)above_020,
                  (double)CONF_THRESHOLD, (unsigned)above_threshold,
                  (double)g_out.scale, g_out.zero_point);
    for (int i = 0; i < 3; ++i) {
        if (top[i].score < 0.0f) {
            continue;
        }
        const char* name =
            top[i].cls < MODEL_NUM_CLASSES
                ? MODEL_CLASSES[top[i].cls].name
                : "?";
        Serial.printf("[infer] top%d class=%s score=%.3f box_raw=(%.1f,%.1f,%.1f,%.1f)\n",
                      i + 1, name, top[i].score, top[i].cx, top[i].cy, top[i].w, top[i].h);
    }
    return kept;
}

#else // ENABLE_TFLITE_MICRO == 0 — stub mode

bool infer_init() {
    Serial.println("[infer] TFLite Micro is disabled in this build.");
    Serial.println("[infer] To enable real inference, build the TFLM environment:");
    Serial.println("[infer] pio run -e seeed_xiao_esp32s3_tflm");
    Serial.println("[infer] See TFLITE_MICRO_EXPORT_NOTES.md for current status.");
    return false;
}

std::vector<Detection> infer_run(const int8_t* /*input*/) {
    // Stub: no model loaded → no detections.
    return {};
}

#endif // ENABLE_TFLITE_MICRO
