# PesoLens Tech Stack

> The 2026-10-04 section at the top of `CURRENT_CHECKPOINT.md` is the current
> deployment source of truth. The 256px/5-class model is flashed; its live
> inference latency still needs to be measured.

Last updated: 2026-10-04

## Current State (2026-10-04)

- **Multi-bill recognition enabled** via `include/config.h` `SINGLE_BILL_MODE 0`.
  No retraining — YOLOv8n is a multi-object detector; the firmware already summed
  all detections and announced the total. See `firmware/PROGRESS.md`.
- **256px/5-class model flashed and boot-verified on COM3.** Camera healthy
  (`OV3660`), model tensors `[1,256,256,3]` to `[1,9,1344]`, audio initialized,
  and the D6 manual-capture ready state was reached.
- Drive PlatformIO via its bundled Python — user-profile `pio.exe` is blocked by a
  Windows Application Control policy:
  `& 'C:\.platformio\penv\Scripts\python.exe' -m platformio <args>`.
- Not yet done: a physical 256px capture, live latency measurement, and
  representative single-/multi-bill accuracy checks.

## IDE And Project Tools

- IDE/editor: Visual Studio Code (`.vscode` project folders are present)
- Firmware build/upload: PlatformIO Core, used from PowerShell
- Firmware cache/toolchain path: `C:\.platformio`
- Serial/debug tools:
  - PlatformIO device listing and upload
  - `tools\monitor_detected_frames.py`
  - `tools\serial_capture.py`
  - Espressif `esptool.py`
- Version-control tooling: Git is expected for source control, but it was not on
  PATH during the 2026-06-01 desktop check.

## Firmware / Embedded Stack

- Target board: Seeed Studio XIAO ESP32-S3 Sense
- MCU: ESP32-S3, 240 MHz
- Memory: 8 MB PSRAM, 8 MB flash
- Camera: XIAO Sense camera module, OV3660 observed in previous healthy logs
- Audio output: MAX98357A I2S amplifier and speaker/bone-conduction transducer
- Status LED:
  - Current deployment build: XIAO `D6 / GPIO43`
  - Manual debug build: XIAO `D5 / GPIO6`, because D6 is the manual capture button there
- Firmware frameworks:
  - Arduino for ESP32
  - ESP-IDF mixed mode
- Firmware language: C / C++
- Firmware filesystem: SPIFFS for WAV audio clips
- Main firmware libraries/components:
  - `espressif/esp32-camera`
  - `espressif/esp-tflite-micro`
  - `espressif/esp-nn`
  - ESP-IDF I2S driver
  - Arduino `FS` / `SPIFFS`
  - `mbedtls` base64 helpers for debug frame dumps
- Host-side firmware tests:
  - PlatformIO `native` + Unity for pure C++ logic tests
  - On this PC, native tests need `gcc/g++` installed or added to PATH.

## Firmware Environments

- `seeed_xiao_esp32s3_tflm_button_capture`
  - USB/debug firmware.
  - Uses D6/GPIO43 as an active-low manual capture button.
  - Uses D5/GPIO6 as the ready LED.
- `seeed_xiao_esp32s3_tflm_button_capture_dump`
  - Debug firmware that streams captured JPEG frames over serial.
- `seeed_xiao_esp32s3_tflm_auto_run_d6_led`
  - Standalone deployment firmware.
  - Starts scanning automatically after power-on.
  - Does not require PowerShell, a serial monitor, or a D6 button press.
  - Uses D6/GPIO43 as the status LED.

## Model / AI Stack

- Current model type: YOLOv8n object detector
- Current input size: `256 x 256`
- Current classes: `20`, `50`, `100`, `500`, `1000`
- Current model does not include `200`.
- Current training platform: Kaggle Notebook
- Current training run: **2026-09-28 imgsz 256 retrain**
  - Merge two Roboflow exports, remap 9 classes down to the 5 firmware denominations
  - Train Ultralytics YOLOv8n at `imgsz=320`, 100 epochs, `batch=16`
  - Full-rotation augmentation (`degrees=180`, `flipud=0.5`); low hue jitter
    (`hsv_h=0.01`) to preserve denomination color
  - Export full-integer INT8 TFLite
  - Embed `.tflite` bytes into `firmware\src\model_data.cpp`
- Current firmware model artifact:
  - `firmware\model_exports\kaggle-yolov8n-imgsz256-5class-20260928\best_full_integer_quant.tflite`
  - 3,229,488 bytes; sha256 begins `efacda433aac255f`
  - Verified tensors: input `[1,256,256,3] int8`, output `[1,9,1344] int8`
- Current `.pt` model artifact:
  - `PesoLens-training\pesolens_best.pt` (sha256 begins `be33cbb7715e31fa`)
- Current model, flashed 2026-10-04: **imgsz 256**, same data and settings
  - Trained on Kaggle (`pesolens-img256` Version 1, Ultralytics 8.4.90, 100 epochs)
  - Exported on Kaggle CPU with **Ultralytics 8.4.76** (`pesolens-img256-export` Version 2).
    Ultralytics ≥8.4.83 exports float32 NCHW TFLite, which the firmware cannot use.
    Preinstall `onnx2tf<1.29`, `onnx_graphsurgeon`, `sng4onnx`, `tf_keras<=2.19`,
    `ai-edge-litert` from PyPI; the Ultralytics auto-install via pypi.ngc.nvidia.com failed.
  - `firmware\model_exports\kaggle-yolov8n-imgsz256-5class-20260928\best_full_integer_quant.tflite`
    (3,229,488 bytes; sha256 begins `efacda433aac255f`; input `[1,256,256,3] int8`, output `[1,9,1344] int8`)
  - INT8 val mAP50 0.954, mAP50-95 0.794

## Training / Conversion Libraries

The training environment is documented in `requirements-training-current.txt`.
Key packages include:

- Python
- Ultralytics YOLO (`ultralytics`)
- PyTorch (`torch`, `torchvision`)
- TensorFlow / Keras (`tensorflow`, `keras`, `tf_keras`)
- TensorBoard
- ONNX and conversion tooling:
  - `onnx`
  - `onnxruntime`
  - `onnx2tf`
  - `onnx_graphsurgeon`
  - `onnxslim`
  - `sng4onnx`
- AI Edge LiteRT (`ai-edge-litert`)
- OpenCV (`opencv-python`)
- Pillow
- NumPy
- SciPy
- Matplotlib
- PyYAML

## Dataset / Annotation Tools

- Roboflow YOLO exports were used for the earlier/new-bill and old-bill datasets.
- Kaggle was used to merge, train, and export the latest 5-class model.
- Local scripts under `PesoLens-training\` and `firmware\tools\` handle prediction,
  export, model embedding, serial capture, and debug image collection.

## Model Training History

- Earlier model / new-bills work:
  - User-provided project history: trained with TensorFlow on an RTX 5090 GPU.
  - The project also retains TensorFlow/TFLite export artifacts under
    `PesoLens-training\pesolens_best_saved_model`.
  - Older handoff notes mention RTX 5090 GPU acceleration and about 25 ms/image
    inference in that local GPU context.
- 2026-06-25 model (superseded, still on disk):
  - Known issue: detected only P1000 in `test_bill.jpg`, missing the visible P500
    even at `--conf 0.10`. Root cause was a weak P500 class (recall `0.41`).
  - Do not embed this. `convert_model.py` now prefers the 20260928 imgsz-256 export.
- Previous model (2026-07-07 thesis2, superseded on-device by imgsz 256):
  - Trained on Kaggle with Ultralytics YOLOv8n, `imgsz=320`, 100 epochs, best at
    epoch 98, on 5024 images (3891 train / 567 valid / 566 test).
  - Exported to full-integer INT8 TFLite for ESP32-S3 firmware.
  - mAP50 `0.969`, mAP50-95 `0.835`, recall `0.933`, precision `0.976`.
  - Per-class recall: `20=0.86`, `50=0.96`, `100=0.95`, `500=0.99`, `1000=0.97`.
    No denomination-to-denomination confusion. Weakest class is now P20.
  - Flashed and boot-verified on device 2026-07-27 (COM3). It was replaced by
    the 256px model on 2026-10-04.
    Camera healthy. Full detection accuracy on real bills still needs live
    testing. See `CURRENT_CHECKPOINT.md`.
  - Measured on-device runtime was 72.9 s/frame.
- Current model (2026-09-28 imgsz 256, flashed 2026-10-04):
  - INT8 validation: mAP50 `0.954`, mAP50-95 `0.794`, precision `0.955`,
    recall `0.918`.
  - Boot-verified on COM3 with the expected INT8 tensors. Physical bill accuracy
    and on-device latency remain to be measured.

## Programming Languages

- C / C++: ESP32-S3 firmware, TFLite Micro integration, camera/audio logic
- Python: model training/export scripts, serial monitoring, model conversion,
  host tooling
- PowerShell: local build/upload/run commands on Windows
- YAML: PlatformIO config, YOLO dataset config, ESP-IDF component manifests
- CSV: ESP32 partition table
