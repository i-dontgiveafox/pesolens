# PesoLens Firmware Changelog

This file is the backtrack log for the firmware folder. Keep it updated when the
model, firmware build, flash layout, or hardware test status changes.

## 2026-10-13 - Reset-reason diagnostics + press acknowledgement beep

Built (`seeed_xiao_esp32s3_tflm_button_capture` SUCCESS); **not yet flashed**, because the
board was not connected.

- Boot logs `[boot] reset reason: ...` from `esp_reset_reason()`.
- Boot self-test plays **3 beeps** after a crash, watchdog or brownout reset, and 1 beep
  otherwise. The user can now tell the reset cause by ear with no serial monitor.
  A reset via EN/RST reads as POWERON (1 beep) on the ESP32-S3.
- A D6 press plays a beep immediately. The beep time comes out of the 3 s aim delay,
  so capture timing is unchanged.
- Why: the client video shows press → LED on → one beep in < 8 s. That matches the boot
  sequence (5 s serial wait with no USB host, plus ~3.6 s init), not a scan
  (≥ 60 s, two sounds). So the press is restarting the board.

## 2026-06-09 - All-Denomination Recognition Audit

- Verified the embedded TFLite is byte-for-byte identical to the current
  224x224 merged six-class export.
- Verified firmware denomination order matches model metadata:
  `20, 50, 100, 200, 500, 1000`.
- Consolidated denomination values and names into one `MODEL_CLASSES` table
  used by inference, counting, and logs.
- Enabled one-bill result selection so duplicate detections of any denomination
  cannot be summed into a wrong larger amount.
- Added per-denomination maximum score logging and input quantization checks.
- Added a boot-time class map and exact speech tests for all six denominations.
- Moved per-capture JPEG dumping after the current result and prevented stale
  result labels in `monitor_detected_frames.py`.
- Expanded `tools/diagnose_tflite_capture.py` for float/INT8 models,
  configurable class order, YOLO dataset replay, and optional HEIC support.
- Kept center-crop preprocessing after saved-camera comparisons showed
  letterboxing did not improve the full-source result.
- Full deployed-model source replay: P20 `0/60`, P50 `329/331`, P100
  `273/274`, P200 `44/60`, P500 `33/150`, P1000 `40/120`; overall
  `719/995` (`72.3%`).
- The older four-class model scored `665/665` on the same complete New Bills
  source set. Float32 and INT8 six-class exports fail similarly, locating the
  broad regression in the six-class training/model lineage.
- Independent 200-image replay scored P100 `69/69`, P1000 `6/29`, P50
  `70/71`, and P500 `11/31` (`78.0%` overall).
- Found that the Kaggle merge omitted all 322 HEIC New Bills images. Updated
  the retraining guide to convert HEIC, assert complete class counts, avoid
  hard-coded run suffixes, and require all-denomination acceptance testing.
- Current six-class model remains embedded for diagnosis but is not
  release-ready.
- Built `seeed_xiao_esp32s3_tflm_button_capture` successfully:
  RAM `22,660` bytes (`6.9%`), flash `3,883,941` bytes (`73.2%`).
- Python diagnostic compilation passed. Native speech tests remain blocked by
  missing desktop `gcc`/`g++`. No board was connected for upload/runtime
  verification.

## 2026-05-31 - D5 Ready LED Added In Firmware

Status: built, uploaded, and runtime-tested on the ESP32-S3 on `COM3`.

- Added ready indicator LED support:
  - XIAO `D5 / GPIO6`
  - active-high by default
  - configured in `include\config.h` as `READY_LED_PIN = 6`
- Manual D6 capture behavior now drives the LED:
  - LED ON when the firmware is initialized and waiting for a D6 capture
  - LED OFF immediately after D6 is pressed and while the board is busy with the
    3000 ms aim delay, camera capture, and TFLite inference
  - after inference, LED blinks 3 times with 1-second on/off timing before
    result/error audio playback
  - LED ON again after the result/error audio path and the ready beep complete
- Updated `src\main.cpp` to keep the ready LED tied to the same ready state
  announced by `[ready] beep`.
- Build/upload command completed successfully:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2 --target upload --upload-port COM3
```

- PlatformIO reported:
  - RAM: `22,668 / 327,680 bytes` (`6.9%`)
  - Flash: `3,882,717 / 5,308,416 bytes` (`73.1%`)
  - upload verified and hard reset completed
- Runtime boot check passed in:
  `logs\serial-reset-test.log`
- D6 manual capture flow check passed in:
  `logs\d6-capture-test.log`
- Verified runtime flow:
  - boot reached `[ready] D5/GPIO6 LED on - ready for capture`
  - D6 press started capture
  - inference completed in about `32159 ms`
  - result was `P200` from `1` bill for that test frame
  - LED blink log appeared before speech:
    `[ready] D5/GPIO6 LED blink 3 times before audio`
  - audio sequence and ready beep logs appeared
  - firmware returned to `[scan] manual mode - waiting for D6/GPIO43 press`

## 2026-05-31 - D6 Frame-Dump Checkpoint and New 1000 Bill Failure

Status: current board firmware at pause is the D6 manual-capture dump build.

- Active uploaded PlatformIO environment:
  `seeed_xiao_esp32s3_tflm_button_capture_dump`
- Latest verified port:
  `COM3`
- The dump build streams each captured frame as JPEG/base64 over serial. The host
  helper saves images under:
  `logs\detections`
- Host command used:

```powershell
py tools\monitor_detected_frames.py --port COM3 --reset --seconds 180 --log logs\image-capture-d6.log
```

- Later capture command/log:
  `logs\next-image-capture.log`
- Latest useful saved frames include:
  - `logs\detections\capture-20260531-022710-P0-0bills-004.jpg`
  - `logs\detections\capture-20260531-023242-P100-1bill-002.jpg`
  - `logs\detections\capture-20260531-023430-P0-0bills-002.jpg`
- The user confirmed the visible bill is a new `1000` peso bill.
- The current 224 YOLO model failed on this real new 1000 bill, producing
  `P200`, `P100`, and `P0/no bill` results across captures.
- Runtime remains about `32.18-32.20 s` per inference after the 3000 ms D6 aim
  delay.
- Next model step: collect many ESP32 camera captures of this new 1000 bill and
  retrain, or continue with `FUTURE_CLASSIFIER_MODEL_PLAN.md`. Do not treat the
  current YOLO model as demo-reliable for new 1000 bills.

Audio/debug changes during this checkpoint:

- Added temporary diagnostic firmware:
  - `src\audio_probe.cpp`
  - `platformio.ini` env `audio_probe`
- `src\audio.cpp` now sends mono WAV samples to both I2S left and right
  channels to avoid silence from MAX98357A channel-select wiring.
- Serial logs show audio init and clip streaming, but physical audio output was
  not confirmed before pause.

To restore the normal non-dump D6 firmware:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2 --target upload --upload-port COM3
```

## 2026-05-30 - D6 Manual-Capture Firmware Uploaded

Status: current active firmware on the ESP32-S3 is the D6 manual-capture TFLite build.

- Active PlatformIO environment:
  `seeed_xiao_esp32s3_tflm_button_capture`
- Latest verified port:
  `COM3`
- Capture button:
  XIAO `D6 / GPIO43` to `GND`, active-low
- Manual flow:
  press D6 -> wait 3000 ms -> capture one frame -> run YOLO/TFLite Micro -> speak amount or error tone -> play ready beep
- `include/config.h` now defines:
  - `CAPTURE_BUTTON_PIN = 43`
  - `MANUAL_CAPTURE_DELAY_MS = 3000`
- `platformio.ini` now includes:
  - `seeed_xiao_esp32s3_tflm_button_capture`
  - `seeed_xiao_esp32s3_tflm_button_capture_dump`
- The normal uploaded build does not dump JPEG frames. Use the `_dump` environment only when a serial JPEG frame capture is needed for visual debugging.
- Firmware upload succeeded on `COM3`.
- PlatformIO reported flash usage around:
  `3,882,233 / 5,308,416 bytes`, about `73.1%`.
- Latest serial boot check confirmed:

```text
[mode] D6/GPIO43 manual capture enabled
[manual] D6/GPIO43 button enabled - press to start 3000 ms aim delay
[cam] init ok (RGB565 QVGA 320x240)
[infer] ready - input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980
[audio] init ok (16 kHz, 16-bit, mono)
[scan] manual mode - waiting for D6/GPIO43 press
```

Remaining proof:

- Run controlled real-bill tests through the D6 manual-capture flow.
- Save per-denomination serial logs and compare top candidate scores/detections.
- If the 32-35 second YOLO inference time is still too slow, continue with the deferred classifier plan in `FUTURE_CLASSIFIER_MODEL_PLAN.md`.

## 2026-05-30 - Kaggle 224 Merged 6-Class Model Embedded

Status: model export and firmware embedding are done. Build/upload/runtime init have since been verified by the D6 manual-capture firmware entry above.

- Kaggle training completed with YOLOv8n, merged Old Bills + New Bills, `imgsz=224`, 50 epochs.
- Current class order:
  `20, 50, 100, 200, 500, 1000`
- Validation result copied from Kaggle:
  - all classes: `P=0.990`, `R=0.937`, `mAP50=0.965`, `mAP50-95=0.909`
  - weakest class: `1000 peso`, recall around `0.673`
- Training outputs were saved under:
  `..\PesoLens-training\runs\pesolens_merged_yolov8n_img224_kaggle`
- TFLite export was moved into:
  `model_exports\kaggle-yolov8n-imgsz224-merged-6class`
- Firmware uses:
  `model_exports\kaggle-yolov8n-imgsz224-merged-6class\best_full_integer_quant.tflite`
- Exported full-integer TFLite size:
  `3,227,889 bytes`
- `include/config.h` was updated to:
  - `MODEL_INPUT_SIZE = 224`
  - `MODEL_NUM_CLASSES = 6`
  - denominations/classes `20, 50, 100, 200, 500, 1000`
- `src/model_data.cpp` was regenerated from the new full-integer TFLite.
- Embedded model length check:
  `tflite_model_len = 3227889`, byte literal count `3227889`
- `tools/convert_model.py` now defaults to the new 224/6-class TFLite export.
- Backup before these firmware edits:
  `backups\before-img224-6class-20260530-052707`

Older proof command kept for reference:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR = "C:\.platformio"
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target upload --upload-port COM3
pio device monitor --port COM3 -b 115200
```

Watch for:

```text
[infer] ready
arena=...
used=...
[infer] invoke=... ms
```

If runtime remains too slow, prefer the deferred tiny-classifier plan in `FUTURE_CLASSIFIER_MODEL_PLAN.md`. A 160 YOLO export remains a fallback experiment.

## 2026-05-26 - TFLite Micro Firmware Build and Upload Verified

Status: real TFLite Micro firmware now builds and uploads to `COM4`; runtime
inference still needs a button-wake test.

- The no-space build path used was `C:\pesosfw` with
  `PLATFORMIO_CORE_DIR=C:\.platformio`.
- `pio run -e seeed_xiao_esp32s3_tflm -j 2 --target upload --upload-port COM4`
  completed successfully.
- Generated TFLite firmware artifact:
  `.pio/build/seeed_xiao_esp32s3_tflm/firmware.bin`
- TFLite firmware size:
  `3,762,928 / 5,308,416 bytes`
- The previous TFLite link failure was from `esp32-camera` being linked as a
  PlatformIO library in the mixed Arduino + ESP-IDF environment. That library
  build excluded the ESP32-S3 low-level camera sources and failed with missing
  symbols such as `ll_cam_start` and `xclk_timer_conf`.
- Fix applied:
  - Recovery/stub env `seeed_xiao_esp32s3` keeps `lib_deps = espressif/esp32-camera`.
  - TFLite env `seeed_xiao_esp32s3_tflm` uses the ESP-IDF component-manager
    dependency from `src/idf_component.yml` instead.
  - `src/idf_component.yml` now includes both `espressif/esp-tflite-micro` and
    `espressif/esp32-camera`.
- Serial capture after upload showed the firmware running and printing:
  `[debug] alive ...`
- Because `PESOS_DEBUG_NO_SLEEP` is still enabled, first boot stays in the
  debug heartbeat loop after the non-button wake path. This proves upload and
  boot, but it does not prove the real bill-counting inference path yet.

Next runtime proof:

```powershell
cd C:\pesosfw
$env:PLATFORMIO_CORE_DIR = "C:\.platformio"
pio device monitor --port COM4 -b 115200
```

Then test a button-wake/capture path after adjusting the debug sleep behavior as
needed. The expected successful TFLite init line is:

```text
[infer] ready - input [1,160,160,3] int8, output ...
```

## 2026-05-26 - ESP32-S3 Replacement Board Bring-Up

Status: verified for stub firmware upload.

- Replacement ESP32-S3 board was detected on `COM4`.
- Firmware upload and SPIFFS/audio upload were verified.
- Serial monitor showed the firmware booting and printing `[debug] alive ...`.
- `PESOS_DEBUG_NO_SLEEP` is still enabled in `platformio.ini` so the board stays
  awake for USB testing.
- The partition table was fixed so `app0` starts at `0x10000`, matching the
  PlatformIO ESP32 Arduino upload offset.

## 2026-05-26 - Kaggle YOLOv8n INT8 Model Added

Status: model file is now small enough for the 8 MB ESP32-S3 flash layout.

- Kaggle training finished with YOLOv8n, `imgsz=160`, 50 epochs.
- Validation result copied from Kaggle:
  - all classes: `P=0.998`, `R=1.000`, `mAP50=0.995`, `mAP50-95=0.987`
  - `100`: `mAP50-95=0.987`
  - `1000`: `mAP50-95=0.991`
  - `50`: `mAP50-95=0.987`
  - `500`: `mAP50-95=0.983`
- Exported model copied into:
  `model_exports/kaggle-yolov8n-imgsz160-balanced/best_int8.tflite`
- Exported model size: `3,160,608 bytes`.
- `include/config.h` now uses `MODEL_INPUT_SIZE = 160`.
- `tools/convert_model.py` now prefers the Kaggle `best_int8.tflite`.
- `src/model_data.cpp` was regenerated locally from the TFLite file.

## 2026-05-26 - TFLite Micro Enable Attempt

Status: build path added, but real TFLite firmware is not proven yet.

What was added:

- `platformio.ini` now has a separate environment:
  `seeed_xiao_esp32s3_tflm`
- `CMakeLists.txt` was added for ESP-IDF mixed-mode builds.
- `src/CMakeLists.txt` was added so ESP-IDF can register the firmware source
  files and include `src/model_data.cpp` when it exists locally.
- `src/idf_component.yml` was added with:
  `espressif/esp-tflite-micro: "*"`
- `src/inference.cpp` comments and stub-mode serial instructions were updated
  for the current 160x160, 4-class model.

What is missing now:

- The normal `seeed_xiao_esp32s3` build still uses stub inference and is the
  recovery build.
- The real TFLite Micro build still needs a successful ESP-IDF mixed-mode build.
- First TFLite build failed before compiling because PlatformIO/ESP-IDF detected
  whitespace in the project/user path:
  `Error: Detected a whitespace character in project paths.`
- This happened because the older project path contained whitespace. The current
  path `C:\Users\james\Projects\PesoLens-project\firmware` does not have that
  specific whitespace issue.

Workaround already started:

- A no-space PlatformIO core folder exists at `C:\.platformio`.
- The first build through the workaround timed out during first-time setup, so it
  needs to be rerun. A timeout at this stage does not prove the firmware failed;
  it only means PlatformIO did not finish the long ESP-IDF setup/build in that
  command window.
- Use a temporary no-space drive alias before building TFLite Micro:

This workaround should no longer be needed from the current project path. If a
future path does contain whitespace, use a no-space folder or drive alias before
running PlatformIO.

If that build succeeds, upload and watch serial logs:

```powershell
pio run -e seeed_xiao_esp32s3_tflm --target upload --upload-port COM4
pio device monitor --port COM4 -b 115200
```

Expected successful TFLite serial sign:

```text
[infer] ready - input [1,160,160,3] int8, output ...
```

If it fails after compiling starts, the next thing to check is whether the
Ultralytics TFLite export uses any operators missing from TFLite Micro.

Verification kept:

- After adding the TFLite build path, the normal recovery build still passes:
  `pio run -e seeed_xiao_esp32s3`
- Latest checked size for that recovery build:
  `Flash: 383,841 / 5,308,416 bytes`
