# PesoLens TFLite Micro Export Notes

Date: 2026-05-30

## Current ESP32-S3 Candidate

The current firmware model is the Kaggle-trained merged 6-class YOLOv8n export:

```text
model_exports/kaggle-yolov8n-imgsz224-merged-6class/best_full_integer_quant.tflite
```

Size:

```text
3,227,889 bytes, about 3.08 MiB
```

The export metadata says:

```text
task: detect
imgsz: [224, 224]
classes: 20, 50, 100, 200, 500, 1000
int8: true
nms: false
```

The TFLite was embedded into:

```text
src/model_data.cpp
```

Embedded length check:

```text
tflite_model_len = 3227889
byte literal count = 3227889
```

The firmware config now matches this export:

```text
MODEL_INPUT_SIZE = 224
MODEL_NUM_CLASSES = 6
Class order = 20, 50, 100, 200, 500, 1000
```

The active uploaded firmware environment is now:

```text
seeed_xiao_esp32s3_tflm_button_capture
```

It uses XIAO `D6 / GPIO43` as an active-low manual capture button. Pressing D6 starts a 3000 ms aim delay, captures one frame, runs inference, plays the result or error tone, then plays a ready beep.

## Why File Size Stayed Similar To 160

The `.tflite` file mostly stores model architecture and weights. YOLOv8n has roughly the same number of weights at `imgsz=160` and `imgsz=224`, so flash size is similar.

Runtime cost is different:

```text
160x160 = 25,600 pixels
224x224 = 50,176 pixels, about 1.96x heavier
```

So the 224 model is likely slower and may need more tensor arena memory even though the file is about the same size.

## Older Reference Exports

The previous ESP32-targeted candidate was:

```text
model_exports/kaggle-yolov8n-imgsz160-balanced/best_int8.tflite
3,160,608 bytes
```

Its full-integer export was about 3.07 MiB and used:

```text
MODEL_INPUT_SIZE = 160
MODEL_NUM_CLASSES = 4
Class order = 100, 1000, 50, 500
```

Older local exports from the larger laptop model were about 26 MB and did not fit the ESP32-S3 app partition.

## Flash And Runtime Targets

The app partition is about:

```text
5,308,416 bytes
```

The embedded TFLite is about 3.08 MiB, and the D6 manual-capture firmware has been built and uploaded. PlatformIO reported flash usage around `3,882,233 / 5,308,416 bytes`, about `73.1%`.

Runtime checks matter more now:

```text
[mode] D6/GPIO43 manual capture enabled
[cam] init ok (RGB565 QVGA 320x240)
[infer] ready - input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980
[scan] manual mode - waiting for D6/GPIO43 press
[infer] invoke=... ms
```

If `AllocateTensors failed`, increase `TENSOR_ARENA_SIZE` in `include/config.h`.

If `invoke` is too slow, the preferred later path is a tiny INT8 classifier as described in `FUTURE_CLASSIFIER_MODEL_PLAN.md`. A 160 YOLO export is still possible, but it is likely less effective than removing YOLO detection overhead for a one-bill-at-a-time workflow.

## Build Commands

Use the current D6 manual-capture TFLite Micro environment:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR = "C:\.platformio"
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2
```

Then upload, replacing `COM3` if `pio device list` shows a different port:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target upload --upload-port COM3
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target uploadfs --upload-port COM3
pio device monitor --port COM3 -b 115200
```

## If Another Export Is Needed

Retrain/export in Kaggle using `PesoLens-training/KAGGLE_RETRAIN_STEPS.md`.

Preferred retry order:

```text
1. Current 224 model: run controlled D6 manual-capture bill tests first.
2. If detection works but runtime is too slow: train a tiny 7-class INT8 classifier.
3. If staying with YOLO temporarily: train/export 160 with the same 6 classes.
```
