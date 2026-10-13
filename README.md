# PesoLens

Assistive AI eyeglasses for visually impaired Filipino users. Press the button on the
glasses, and a bone-conduction speaker says the total of the Philippine peso bills the
camera can see, in English. Everything runs on the device: no internet, no cloud.

- **Board:** Seeed XIAO ESP32-S3 Sense (8 MB PSRAM, OV3660 camera)
- **Model:** YOLOv8n, int8 TFLite Micro, 256×256 input, 5 classes (₱20, ₱50, ₱100, ₱500, ₱1000)
- **Audio:** MAX98357A I2S amp → bone-conduction speaker, pre-recorded English WAV clips in SPIFFS

## Current status (2026-10-13)

| Area | State |
|---|---|
| Model | 256px model trained on Kaggle 2026-09-28. Validation (567 images): mAP50 **0.962**, precision 0.947, recall 0.938. Weakest class: ₱20 recall 0.81. |
| Firmware | 256px model embedded, built, flashed and **boot-verified** 2026-10-04 (camera, PSRAM, TFLite, audio self-test beep all OK). |
| End-to-end scan | **Not yet verified on device.** A full button → inference → spoken amount cycle with the 256px model has not been logged. Client testing reports an error tone on button press; under investigation (see `CURRENT_CHECKPOINT.md`). |
| Inference time | ~47 s estimated at 256px (73 s measured at 320px). Not yet measured. |

Full history and the authoritative current state: **[`CURRENT_CHECKPOINT.md`](CURRENT_CHECKPOINT.md)**.
Moving to another PC: [`START_HERE.md`](START_HERE.md). Tools and versions: [`TECH_STACK.md`](TECH_STACK.md).

## Using the device (current `button_capture` firmware)

1. Power on. The board plays a short **beep** (boot self-test) and the ready LED (D5) turns on.
2. Hold the bill about 20–30 cm in front of the camera and press the **D6** button.
3. The LED turns off. The photo is taken **3 seconds** after the press, so keep the bill in view.
4. Inference runs for roughly **50 seconds** with no sound. This is expected.
5. The LED blinks 3 times, then the device either **speaks the amount** (e.g. "one hundred pesos")
   or plays the **error tone** if it found no bill. A ready beep follows.

## Repository layout

| Folder | Contents |
|---|---|
| [`firmware/`](firmware/) | PlatformIO project for the ESP32-S3: C++ source, build envs, audio clips, model conversion and diagnostic tools. See [`firmware/README.md`](firmware/README.md). |
| [`PesoLens-training/`](PesoLens-training/) | Kaggle training notebooks, dataset merge/export scripts, local validation and prediction scripts, training metrics and plots per run. |
| [`docs/`](docs/) | Setup guide, hardware/wiring, deployment, audio clip list. Written for the original May build; check against `CURRENT_CHECKPOINT.md`. |

## Not in this repository

These files are too large for GitHub and are excluded in `.gitignore`. Keep backups on Kaggle or Google Drive.

- Model weights (`*.pt`, `*.onnx`, `*.tflite`) and the generated `firmware/src/model_data.cpp`
- Datasets (Roboflow exports, the merged ~5,100-image training set, dataset zips)
- Python virtualenv, PlatformIO build output, ESP-IDF managed components

To rebuild the firmware on a fresh clone, put the int8 TFLite export at
`firmware/model_exports/kaggle-yolov8n-imgsz256-5class-20260928/best_full_integer_quant.tflite`
and run `firmware/tools/convert_model.py`.
