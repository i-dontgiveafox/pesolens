# PesoLens Project Start Here

This folder is the portable project copy for continuing PesoLens work on another PC.

Last prepared: 2026-10-04

## Current state

Read the **2026-10-04** section at the top of `CURRENT_CHECKPOINT.md`. It is the
single source of truth. In short: the validated 256px/5-class model is embedded,
built, flashed, and boot-verified on COM3. Camera, PSRAM, TFLite, audio, and D6
manual-capture readiness all initialized successfully. A physical D6 capture is
still needed to measure 256px inference latency and validate real bills.
Everything below here is history.

## 2026-07-27 State (superseded)

Summary:

- **Multi-bill recognition is now ON.** `include/config.h` `SINGLE_BILL_MODE` was
  changed `1 -> 0`. No retraining was needed — YOLOv8n already detects every bill;
  the firmware already sums all detections and speaks the total. The old flag was
  just discarding all but the strongest box. Double-counting one bill is still
  prevented by `CLASS_AGNOSTIC_NMS 1`.
- **The 320px/5-class model is flashed and boot-verified on COM3** for the first
  time. Upload used a normal `--target upload` (esptool reset works; the old
  "upload blocked" note is stale). The camera is healthy (`Detected OV3660`,
  no more `0x105`). Boot log: `logs/multibill-boot-verify.log`.
- Fixed a serial-log clarity bug: the result banner now reports bill count +
  total + per-denomination breakdown instead of a single class name.
- **Not yet done:** a physical test with 2+ real bills (press D6, hold two
  non-overlapping bills, expect `[result] N pesos from M bills`, M>1). Inference
  is still ~60-70 s/scan at 320px. Speaker output still unconfirmed.
- To drive PlatformIO on this PC, use its bundled Python (the user-profile
  `pio.exe` is blocked by a Windows Application Control policy):
  `& 'C:\.platformio\penv\Scripts\python.exe' -m platformio <args>`.

## Superseded 2026-07-10 State

Read `CURRENT_CHECKPOINT.md` first. Summary:

- The firmware embeds the **2026-07-07 thesis2 model**, not the 20260625 model
  that the sections below describe. Verified by hashing the byte array in
  `firmware/src/model_data.cpp` against the export files.
- That model is good: mAP50 `0.969`, and **P500 recall `0.99`** (the 20260625
  model's `0.41` P500 recall is the "misses the visible P500" bug referenced
  throughout the older sections).
- Embedded tensor contract verified: input `[1,320,320,3] int8`, output
  `[1,9,2100] int8`. Matches `MODEL_INPUT_SIZE 320` / `MODEL_NUM_CLASSES 5`.
- Firmware **builds** (Flash 73.3%) but has **not been uploaded**. esptool cannot
  configure `COM3` because of the native USB-Serial-JTAG reset behavior. Enter ROM
  download mode by hand (hold **BOOT**, tap **RESET**, release **BOOT**), then
  flash with `--before no_reset`. Full command in `CURRENT_CHECKPOINT.md`.
- The 2026-06-01 camera fault (`0x105 ESP_ERR_NOT_FOUND`) is still unresolved.
- Expect ~60-70 s per inference at 320px (extrapolated from 32-35 s at 224px).

## Superseded 2026-06-25 Kaggle 5-Class Model Import

> **SUPERSEDED.** The firmware does not embed this model. See above.

- Imported Kaggle zips from Downloads into
  `PesoLens-training\kaggle_exports\20260625`.
- Active local Python model is now:
  `PesoLens-training\pesolens_best.pt`.
- Active firmware export folder is now:
  `firmware\model_exports\kaggle-yolov8n-imgsz320-5class-20260625`.
- Firmware config now expects a 320px, five-class model:

```text
MODEL_INPUT_SIZE = 320
MODEL_NUM_CLASSES = 5
classes = 20, 50, 100, 500, 1000
```

- This model does not include `200`.
- `firmware\src\model_data.cpp` was regenerated from
  `best_full_integer_quant.tflite` and byte-literal verification passed.
- Local Python prediction works and saves annotated images, but the included
  `test_bill.jpg` currently detects only the P1000 even though the image also
  shows a P500. Validate with more sample photos before treating the new model
  as reliable.

## Previous 2026-05-31 D5 Ready LED Tested Checkpoint

- Current board port is still `COM3`.
- Current uploaded firmware is now `seeed_xiao_esp32s3_tflm_button_capture`,
  the normal D6 manual-capture build with D5/GPIO6 ready LED behavior.
- D5/GPIO6 LED behavior:
  - ON when ready for D6 capture
  - OFF after D6 is pressed
  - blinks 3 times with 1-second ON/OFF timing after inference, before audio
  - ON again after result/error audio and the ready beep
- Runtime boot and one full D6 capture cycle were verified.
- Latest verification logs:
  - `firmware\logs\serial-reset-test.log`
  - `firmware\logs\d6-capture-test.log`
- Quick test command for next session:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
py tools\monitor_detected_frames.py --port COM3 --reset --seconds 120 --log logs\quick-test.log
```

- Use `CURRENT_CHECKPOINT.md` and `firmware/PROGRESS.md` first.
- Latest image captures are in:
  `firmware\logs\detections`
- Latest useful logs are:
  `firmware\logs\serial-reset-test.log`,
  `firmware\logs\d6-capture-test.log`,
  `firmware\logs\image-capture-d6.log`, and
  `firmware\logs\next-image-capture.log`.
- Previous frame-dump image collection used
  `seeed_xiao_esp32s3_tflm_button_capture_dump`.
- A new `1000` peso bill was visibly captured, but the previous 224px model misread it
  as `P200`, `P100`, or no bill.
- Treat the previous 224px YOLO model as not reliable for new 1000 bills. The next
  useful work is collecting more ESP32 camera images for retraining or switching
  to the planned lightweight classifier.
- Audio software logs show playback, and a temporary `audio_probe` env was added,
  but physical speaker output was not confirmed before pause.

## Folder Layout

- `PesoLens-training/`
  - AI/model training project.
  - Contains datasets, YOLO training runs, exported models, prediction outputs, and training scripts.
  - The old larger TFLite exports are here, including the roughly 25 MB model family.

- `firmware/`
  - ESP32-S3 firmware project.
  - Contains PlatformIO firmware source, audio clips, TFLite Micro integration, model export files, serial logs, and current handoff notes.
  - Read `CURRENT_CHECKPOINT.md` and `firmware/PROGRESS.md` before continuing firmware work.

- `CURRENT_CHECKPOINT.md`
  - Current resume checkpoint for the active source tree and model import.

- `requirements-training-current.txt`
  - Python package list from the source laptop's training virtual environment.
  - Use this to rebuild the Python environment on another PC.

## Firmware Working State

- The **2026-09-28** 256px/5-class model is built (Flash 73.1%), **uploaded, and
  boot-verified** on `COM3`. A normal esptool upload works. The 256px inference
  latency has not been measured yet; the older 320px baseline was 72.9 s.
- The historical bullets below describe older 224px/320px testing.

- ESP32-S3 camera works.
- Current trigger is the temporary/user-facing D6 capture button: XIAO `D6 / GPIO43` to `GND`, active-low.
- Current flow is manual capture: press D6, wait 3 seconds, capture one frame,
  run inference, speak the result or error tone, then play a ready beep. The
  normal build does not stream/save a JPEG; use the `_dump` environment for that.
- Audio clips were regenerated from silent placeholders and uploaded to SPIFFS.
- Earlier, a 50 peso bill was successfully detected twice in controlled serial capture.
- Earlier 224px testing focused on a new 1000 peso bill. That model misread it as `P200`, `P100`, or no bill, so it was not reliable for that bill.
- Previous Kaggle merged 6-class YOLOv8n model was trained at `imgsz=224`, exported to full-integer TFLite, embedded into firmware, built, and flashed.
- On-device serial capture confirmed camera init, 224 model init, audio init, and manual D6 waiting state for that older firmware.
- The previous 224px YOLO inference was slow on ESP32-S3: about 32-35 seconds after the 3-second aim delay. The new 320px model may be slower and needs live timing.

## Current Firmware Model

The firmware source and board currently use this 256/5-class model:

```text
firmware/model_exports/kaggle-yolov8n-imgsz256-5class-20260928/best_full_integer_quant.tflite
```

Size: 3,229,488 bytes. SHA-256:
`efacda433aac255ff7eaa72a2683520ad5f97d16e82a508ce7cb5e651e852f80`.

Verified tensors: input `[1,256,256,3] int8` (scale 1/255, zero -128),
output `[1,9,1344] int8`.

The class order is:

```text
20, 50, 100, 500, 1000
```

Firmware config now expects:

```text
MODEL_INPUT_SIZE = 256
MODEL_NUM_CLASSES = 5
```

### 256px deployment (flashed 2026-10-04)

```text
firmware/model_exports/kaggle-yolov8n-imgsz256-5class-20260928/best_full_integer_quant.tflite
```

3,229,488 bytes, sha256 starts `efacda433aac255f`. Input `[1,256,256,3] int8` (scale 1/255,
zero -128), output `[1,9,1344] int8`; passes the firmware contract. INT8 val mAP50 0.954 /
mAP50-95 0.794 (float best.pt 0.962 / 0.818). Goal: cut the 72.9 s scan to about 47 s.
Deployment and provenance are in that folder's `README.md`. The embedded bytes
match the artifact exactly. The manual-capture build passed at 73.1% flash use,
was uploaded to COM3, and booted with the expected tensors. See
`firmware/logs/img256-boot-verify-20261004.log`.

The large older model family is in:

```text
PesoLens-training/pesolens_best_saved_model/
```

## Local Generated Folders

- `PesoLens-training/.venv`
  - Local Python virtual environment for `predict.py`.
  - Rebuild with `PesoLens-training/requirements-inference.txt` if needed.

- `firmware/.pio`
  - PlatformIO build/package cache.
  - It is generated locally by PlatformIO and can be rebuilt.

## Rebuild Python Environment On Desktop

From PowerShell:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\PesoLens-training"
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements-inference.txt
```

## Firmware Commands On Desktop

From PowerShell:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
pio device list
```

Build firmware:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2
```

Upload firmware:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target upload --upload-port COM3
```

Upload audio filesystem if the WAV clips need to be refreshed:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target uploadfs --upload-port COM3
```

Open serial monitor:

```powershell
pio device monitor --port COM3 -b 115200
```

If the desktop uses a different COM port, replace `COM3` with the port shown by `pio device list`.
On the latest desktop check, the board was `COM3`.

## Latest Historical On-Device Check

The D6 manual-capture 224/6-class firmware was built and uploaded to the ESP32-S3 on `COM3` before the 2026-06-25 model import.
SPIFFS audio data was also uploaded.

Latest confirmed runtime lines:

```text
[mode] D6/GPIO43 manual capture enabled
[manual] D6/GPIO43 button enabled - press to start 3000 ms aim delay
[cam] init ok (RGB565 QVGA 320x240)
[infer] ready - input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980
[audio] init ok (16 kHz, 16-bit, mono)
[scan] manual mode - waiting for D6/GPIO43 press
```

Latest saved serial log:

```text
firmware/logs/detection-monitor-20260530-222814.log
```

Use `seeed_xiao_esp32s3_tflm_button_capture_dump` only when a serial JPEG frame dump is needed for visual debugging. The currently uploaded build is the normal manual-capture build and does not dump JPEG frames.

## Prompt For Codex On Desktop

Use this prompt when opening Codex in `C:\Users\james\Projects\PesoLens-project`:

```text
Read the 2026-09-28 section at the top of CURRENT_CHECKPOINT.md first; it overrides every older section in every doc. The 320x320 five-class YOLOv8n INT8 thesis2 model (classes 20, 50, 100, 500, 1000; no 200) is flashed and running on COM3 via a normal esptool upload. The camera is healthy, audio is confirmed, and inference is measured at 72.9 s per scan. Open issues: the D6 button press has never been logged, the scan is too slow, multi-bill false positives inflate totals, 5 envs have stale UART0 sdkconfigs, and the speech clips are quiet.
```
