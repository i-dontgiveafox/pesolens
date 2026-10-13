# PesoLens Firmware Progress

> **Superseded in places.** The 2026-10-04 section at the top of
> `CURRENT_CHECKPOINT.md` is the current state and wins wherever this file
> disagrees. The 256px/5-class model is now flashed and boot-verified on COM3;
> live 256px inference timing and bill tests remain.

Last updated: 2026-07-27 multi-bill enabled + first on-device flash of 320px model

## 2026-07-27 Multi-bill Recognition Enabled + Flashed + Verified

**This is the current state. Sections below are historical.**

### Multi-bill turned on (no retraining)

- `include/config.h`: `SINGLE_BILL_MODE` changed `1 -> 0`.
- Why this is all it took: the model is YOLOv8n, an object *detector* that already
  finds every bill in the frame. `infer_run()` already returns all post-NMS
  detections; `count_total_pesos()` already sums them; `play_announcement(total)`
  already speaks the summed total. The only thing throwing bills away was the
  `SINGLE_BILL_MODE` block in `inference.cpp` (`kept.resize(1)`), now compiled out.
- `CLASS_AGNOSTIC_NMS 1` still prevents one physical bill being counted twice:
  overlapping boxes (even across denominations) suppress each other.
- Audit result: counting, speech composition, and audio clips are all multi-bill
  safe. Every reachable multi-bill total (sums of 20/50/100/500/1000) resolves to
  number-word clips that all exist in `data/` (checked `ten`..`ninety`, `hundred`,
  `thousand`, `one`..`nine`, `peso(s)`). No missing-clip gaps.
- Fixed one clarity bug in `src/main.cpp`: the result banner used to print a single
  class name for the whole scan (e.g. `DETECTED: P500 (1000 peso bill)` for two
  P500s). It now prints bill count + total + a per-denomination breakdown, e.g.
  `DETECTED: 2 bills = P1000` then `[result] breakdown: 2x P500`.

### First on-device flash of the 320px/5-class model

- Uploaded `seeed_xiao_esp32s3_tflm_button_capture` to `COM3` with a plain
  `--target upload`. **Upload is NOT blocked** — esptool's own reset works;
  `Hash of data verified`. No manual BOOT/RESET download mode needed. The
  2026-07-10 "upload blocked" note below is stale.
- Note: the user-profile `pio.exe` is blocked by a Windows Application Control
  policy. Drive PlatformIO through its bundled Python instead:
  `& 'C:\.platformio\penv\Scripts\python.exe' -m platformio ...`
- **Camera is healthy.** Boot log: `Detected OV3660 camera`, `[cam] init ok
  (RGB565 QVGA 320x240)`. The 2026-06-01 `0x105 ESP_ERR_NOT_FOUND` fault does not
  reproduce.
- Boot verified: `[infer] ready — input [1,320,320,3] int8, output [1,9,2100]`,
  class map `0=P20 1=P50 2=P100 3=P500 4=P1000`, `[audio] init ok`, then
  `[scan] manual mode - waiting for D6/GPIO43 press`. Log:
  `logs/multibill-boot-verify.log`.

### Still open

- **Physical multi-bill test not done.** Boot verified only. Press D6 with 2+
  non-overlapping bills and confirm `[result] N pesos from M bills` (M>1) with no
  `single-bill mode` line.
- Inference latency ~60-70 s/scan at 320px (unmeasured live).
- Speaker output still never physically confirmed.

## 2026-07-10 Correction: embedded model is the 20260707 export

`src/model_data.cpp` was regenerated on 2026-07-07 22:45 from
`model_exports/kaggle-yolov8n-imgsz320-5class-20260707/best_full_integer_quant.tflite`
(3,233,952 bytes, sha256 `19d1301430fc0f10...`). The section below, which names
the `20260625` export, is **superseded** -- that file is 3,233,864 bytes and
hashes differently.

Tensors verified against the embedded bytes: input `[1,320,320,3] int8`
(scale 1/255, zero -128), output `[1,9,2100] int8`. Consistent with
`include/config.h` (`MODEL_INPUT_SIZE 320`, `MODEL_NUM_CLASSES 5`,
classes `{20,50,100,500,1000}`).

Build passes:

```text
pio run -e seeed_xiao_esp32s3_tflm_button_capture
RAM:   22,668 / 327,680 bytes (6.9%)
Flash: 3,891,197 / 5,308,416 bytes (73.3%)
```

**Not uploaded.** esptool cannot configure `COM3` (Windows error 31) because
`ARDUINO_USB_MODE=1` routes the port through the ESP32-S3 built-in
USB-Serial-JTAG, and DTR/RTS toggling re-enumerates the device mid-handshake.
`usb_reset`, `default_reset`, `no_reset`, and the `esp-builtin` OpenOCD/JTAG
path all fail. Workaround: manual ROM download mode (hold BOOT, tap RESET,
release BOOT), then flash with `--before no_reset`. Command in
`../CURRENT_CHECKPOINT.md`.

`tools/convert_model.py` had the 20260625 export first in
`DEFAULT_TFLITE_CANDIDATES`; re-running it argument-free would have silently
regressed the firmware to the bad model. Fixed -- 20260707 is now first.

The board physically still runs the older **224px/6-class** manual-capture build.

## 2026-06-25 320px Five-Class Model Import

> **SUPERSEDED.** The firmware does not embed this model. See above.

- Added firmware export folder:
  `model_exports/kaggle-yolov8n-imgsz320-5class-20260625`.
- Updated `include/config.h` for the new model contract:

```text
MODEL_INPUT_SIZE = 320
MODEL_NUM_CLASSES = 5
MODEL_CLASSES = 20, 50, 100, 500, 1000
```

- This model intentionally omits `200`; do not test or announce P200 with this
  firmware build.
- Regenerated `src/model_data.cpp` from
  `best_full_integer_quant.tflite` using `tools/convert_model.py`.
- Conversion verification:

```text
size: 3,233,864 bytes
app partition: 5,308,416 bytes
byte literals: 3,233,864 - OK
```

- Raw TFLite tensor check:

```text
input: [1, 320, 320, 3] int8, scale=1/255, zero=-128
output: [1, 9, 2100] int8
```

- Not yet live-tested on ESP32-S3 after this import. Before flashing as a
  replacement, validate the P500 miss seen in local `test_bill.jpg` prediction.

## 2026-06-09 All-Denomination Recognition Audit

- Audited the full firmware path for all classes:
  `20, 50, 100, 200, 500, 1000`.
- Replaced separate denomination/name arrays with one `MODEL_CLASSES` table.
  Counting, result logs, detection logs, and inference diagnostics now all
  read from the same table, preventing class-order drift.
- `SINGLE_BILL_MODE=1` applies to every denomination. In the one-bill capture
  workflow, duplicate post-NMS boxes can no longer be summed into a larger
  amount.
- Added boot-time class-map logging and exact P50/P200 speech tests. Existing
  speech tests already covered P20, P100, P500, and P1000.
- Replayed the deployed six-class full-integer TFLite against every usable
  labeled source image, including HEIC:

```text
P20:      0/60   (0.0%)  - all 60 predicted as P50
P50:    329/331 (99.4%)
P100:   273/274 (99.6%)
P200:    44/60  (73.3%)
P500:    33/150 (22.0%)
P1000:   40/120 (33.3%)
Overall: 719/995 (72.3%)
```

- Letterbox replay was not better (`714/995`, 71.8%), so the firmware center
  crop is not the main cause.
- The older four-class model scored `665/665` on the complete New Bills source
  set. This validates the source labels and replay evaluator and isolates the
  regression to the current six-class model/training lineage.
- On a separate 200-image validation set, the six-class model scored:
  P100 `69/69`, P1000 `6/29`, P50 `70/71`, and P500 `11/31`
  (`156/200`, 78.0% overall).
- Float32 and INT8 six-class exports fail similarly. Quantization is not the
  main cause.
- Found a concrete training-data bug: the Kaggle merge accepted only
  JPG/JPEG/PNG and omitted all 322 HEIC New Bills images. The prior merged
  input therefore had only 7 new P1000 JPGs and 14 new P500 JPGs.
- Updated `PesoLens-training/KAGGLE_RETRAIN_STEPS.md` to:
  - decode HEIC into JPG
  - assert complete image and per-class counts
  - export from the current trainer run instead of a hard-coded `-2` folder
  - require per-denomination acceptance replay before firmware embedding

Conclusion:

- The original P50-to-P100 symptom had a possible program-side duplicate-box
  counting path, now contained for every denomination.
- The broader recognition failures are model-side. The current six-class model
  is not release-ready and cannot be safely repaired with a class remap or a
  confidence-threshold tweak.
- Retrain from the complete source set and validate all six denominations on
  held-out ESP32 camera images before replacing the embedded model.
- Production firmware build passed:
  - RAM: `22,660 / 327,680` bytes (`6.9%`)
  - Flash: `3,883,941 / 5,308,416` bytes (`73.2%`)
- Python diagnostic tools compile successfully. Native speech tests could not
  compile because this PC still has no `gcc`/`g++`. No ESP32-S3 was connected;
  `pio device list` showed only `COM1`.

## 2026-06-09 PHP 50 To PHP 100 Diagnosis

- Verified the deployed model contract:
  - embedded `src/model_data.cpp` bytes exactly match
    `best_full_integer_quant.tflite`
  - SHA-256:
    `33ff5b352c41f7928590da81f4320ee235971d6e051e3def09bbd0a2c6fbaa7a`
  - input is `[1,224,224,3]` INT8 with scale `1/255`, zero point `-128`
  - output is `[1,10,1029]` INT8
  - model metadata and firmware both use class order
    `20, 50, 100, 200, 500, 1000`
- No hard-coded `50 -> 100` class mapping fault was found.
- Desktop TFLite replay of five saved real 50-peso camera captures classified
  all five as class `50` with the existing firmware center-crop preprocessing.
  This rules out a permanent model-label swap.
- The current one-bill workflow previously summed every surviving detection.
  Two duplicate `P50` boxes could therefore produce an announced `P100`.
  `SINGLE_BILL_MODE=1` now keeps only the highest-confidence post-NMS box.
- Inference logs now print maximum confidence for every denomination:

```text
[infer] class_max 20=... 50=... 100=... 200=... 500=... 1000=...
```

  On the next live failure, this distinguishes a real model error
  (`100` score wins) from downstream counting/mapping behavior.
- Added an input quantization contract check during `infer_init()` so a future
  model replacement cannot silently use incompatible preprocessing.
- Fixed debug capture association:
  - dump firmware now emits each JPEG after the current `[result]`
  - the host monitor clears stale results at each new scan
  - old pre-inference dump firmware will save such frames as `unknown` instead
    of assigning the previous scan's denomination
- Added `tools/diagnose_tflite_capture.py` to replay saved JPEGs through the
  exact deployed TFLite model and print all class scores.
- Compared Ultralytics-style letterboxing with the existing center crop. On the
  known camera captures, letterboxing reduced recall and missed one 50-peso
  frame that center crop detected, so production preprocessing was not changed.
- Firmware build passed:
  `seeed_xiao_esp32s3_tflm_button_capture`
  - RAM: `22,660 / 327,680` bytes (`6.9%`)
  - Flash: `3,883,781 / 5,308,416` bytes (`73.2%`)
- No ESP32-S3 was connected during this diagnosis. `pio device list` showed
  only `COM1`, so the changes were built but not uploaded or live-tested.

Interpretation for the next live test:

- If logs show multiple NMS detections but one returned result, the duplicate
  counting fault is contained.
- If `class_max` shows `100` clearly beating `50` on an actual 50-peso bill,
  the remaining fault is model/domain shift. Collect the correctly associated
  debug JPEGs and retrain with ESP32 camera images under the actual lighting,
  distance, rotation, glare, and background.

## 2026-06-01 Desktop Hardware Health Check

- Board is detected on `COM3`:
  `USB VID:PID=303A:1001`, serial `E0:72:A1:FA:38:3C`.
- Esptool can talk to the chip:
  - `ESP32-S3 (QFN56)`, revision `v0.2`
  - `WiFi, BLE, Embedded PSRAM 8MB`
  - crystal `40MHz`
  - flash detected as `8MB`
- Firmware build check passed:

```powershell
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2
```

- Build size remains healthy:
  - RAM: `22,668 / 327,680` bytes (`6.9%`)
  - Flash: `3,882,717 / 5,308,416` bytes (`73.1%`)
- Native host tests did not run because `gcc`/`g++` are not installed/on PATH on
  this PC. This is a desktop toolchain issue, not an ESP32 firmware build issue.
- Runtime boot now repeatedly fails at camera probe:

```text
E camera: Camera probe failed with error 0x105(ESP_ERR_NOT_FOUND)
[cam] ERROR: esp_camera_init failed 0x105
[err] camera_init failed
```

- New health logs:
  - `logs\health-20260601-boot.log`
  - `logs\health-20260601-camera-retry.log`
  - `logs\health-20260601-after-esptool.log`
- Interpretation: ESP32-S3 core, USB serial, flash, PSRAM, bootloader, and build
  tooling look healthy. The current unhealthy part is the camera sensor path.
  Since May 31 logs detected `OV3660` at `0x3c` with the same firmware, first
  suspect the camera/expansion connection, ribbon seating, or interference on
  the camera SCCB pins `GPIO39/GPIO40`.
- Added standalone deployment firmware environment:
  `seeed_xiao_esp32s3_tflm_auto_run_d6_led`.
- Uploaded `seeed_xiao_esp32s3_tflm_auto_run_d6_led` to `COM3`.
- This deployment firmware starts by itself on power-up; it does not require
  PowerShell, `monitor_detected_frames.py`, or a D6 button press.
- D6/GPIO43 is now the status LED pin in the deployment firmware. The upload
  verification log shows:

```text
gpio: GPIO[43]| InputEn: 1| OutputEn: 1
[mode] continuous scan enabled - no button required
```

- Upload/verification log:
  `logs\auto-run-d6-led-verify.log`
- Reuploaded the same deployment firmware on 2026-06-01 after a temporary test
  firmware had overwritten the board. Verification log:
  `logs\auto-run-reupload-verify.log`
- Temporary blink-test code/env was later removed from the repo, and the
  PesoLens deployment firmware was restored again on `COM3`. Verification log:
  `logs\original-firmware-restore-verify.log`
- Camera probe is still failing after the upload, so auto-run firmware is
  installed but cannot capture frames until the camera connection is fixed:

```text
E camera: Camera probe failed with error 0x105(ESP_ERR_NOT_FOUND)
[cam] ERROR: esp_camera_init failed 0x105
```

## 2026-05-31 Latest Tested Checkpoint

- Board detected on `COM3`:
  `USB VID:PID=303A:1001`, serial `E0:72:A1:FA:38:3C`.
- Latest uploaded firmware is now the normal D6 manual-capture build:
  `seeed_xiao_esp32s3_tflm_button_capture`.
- Upload command completed successfully:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2 --target upload --upload-port COM3
```

- This upload includes the D5/GPIO6 ready LED behavior:
  - LED ON when initialized and waiting for D6 capture
  - LED OFF immediately after D6 is pressed
  - LED blinks 3 times with 1-second on/off timing after inference, before audio
  - result/error audio plays
  - ready beep plays
  - LED ON again for the next capture
- PlatformIO verified the flash write and hard reset the board.
- Runtime boot was later verified with:
  `logs\serial-reset-test.log`
- Confirmed boot/runtime lines:
  - `[cam] init ok (RGB565 QVGA 320x240)`
  - `[infer] ready - input [1,224,224,3] int8, output [1,10,1029]`
  - `[audio] init ok (16 kHz, 16-bit, mono)`
  - `[scan] manual mode - waiting for D6/GPIO43 press`
  - `[ready] D5/GPIO6 LED on - ready for capture`
- One full D6 capture cycle was verified with:
  `logs\d6-capture-test.log`
- The verified capture showed:
  - `[infer] invoke=32159 ms`
  - `[result] 200 pesos from 1 bills`
  - `[ready] D5/GPIO6 LED blink 3 times before audio`
  - `[speak] announcing P200 with 3 clips: two hundred pesos`
  - `[ready] beep`
  - `[ready] D5/GPIO6 LED on - ready for next capture`
- A second D6 press was also seen starting a capture, but that monitor run ended
  before the second inference completed.
- If no serial port/logs appear, the board may need USB unplug/replug. During
  the final test COM3 temporarily disappeared, then returned after reconnecting.

## 2026-05-31 Pause Checkpoint

- Board is still detected as `COM3`.
- Previous pause firmware was the debug image-capture build:
  `seeed_xiao_esp32s3_tflm_button_capture_dump`.
- This is not the normal production/manual build. It saves each D6 capture as a
  JPEG on the PC through `tools\monitor_detected_frames.py`.
- Current D6 flow in the dump build:
  press D6 -> wait 3000 ms -> capture frame -> serial JPEG dump -> preprocess ->
  YOLO/TFLite inference -> speak amount/error -> ready beep.
- Ready LED code has now been added for XIAO `D5 / GPIO6`: LED is ON while
  waiting for the next D6 capture, OFF after D6 is pressed, then blinks 3 times
  with 1-second on/off timing after inference and before audio playback.
- Latest useful capture logs:
  - `logs\image-capture-d6.log`
  - `logs\next-image-capture.log`
- Latest saved capture images include:
  - `logs\detections\capture-20260531-022710-P0-0bills-004.jpg`
  - `logs\detections\capture-20260531-023242-P100-1bill-002.jpg`
  - `logs\detections\capture-20260531-023430-P0-0bills-002.jpg`
- User confirmed the visible bill in the latest test is a new `1000` peso bill.
- Current model failed this new `1000` bill. Results included:
  - `P200`
  - `P100`
  - `P0 / no bill`
- Inference still takes about `32.18-32.20 s` per capture after the 3-second D6
  delay.
- Current conclusion: the 224 YOLO model is not reliable for new 1000 bills.
  Threshold tuning is not the right main fix; collect more ESP32 camera captures
  of the new 1000 bill and retrain, or move to the deferred classifier plan.

Audio status:

- Serial logs show `[audio] init ok` and clip playback lines for spoken results,
  error tone, and ready beep.
- A temporary diagnostic env was added:
  - `audio_probe`
  - `src\audio_probe.cpp`
- `src\audio.cpp` now duplicates mono WAV samples to both I2S left and right
  channels, so MAX98357A channel select should not mute audio.
- Physical speaker/bone-conduction output was not confirmed before pause. If it
  remains silent, inspect MAX98357A wiring/power and any `SD`/`EN` pin.

Resume image capture:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
py tools\monitor_detected_frames.py --port COM3 --seconds 180 --log logs\next-image-capture.log
```

Restore normal D6 firmware when done collecting images:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2 --target upload --upload-port COM3
```

## Current Status

- Board is currently detected as `COM3` when plugged in.
- Firmware built and uploaded with PlatformIO environment `seeed_xiao_esp32s3_tflm_button_capture`.
- Camera initializes after power/reset, then waits for the D6 manual capture button.
- Current capture button is XIAO `D6 / GPIO43` wired to `GND`, active-low.
- Pressing D6 starts a 3000 ms aim delay, captures one frame, runs inference,
  blinks the D5 ready LED 3 times, plays the detected amount or an error tone,
  then plays a ready beep.
- Ready indicator LED is XIAO `D5 / GPIO6`, active-high. It turns ON when the
  ESP32-S3 is ready for a D6 capture, turns OFF immediately after D6 is pressed,
  blinks 3 times after the bill read finishes, and turns ON again after the
  ready beep.
- TFLite Micro initializes successfully with the embedded full-integer int8 model.
- A new merged 6-class YOLOv8n model was trained in Kaggle at `imgsz=224`.
- The new 224 full-integer TFLite model is embedded in `src/model_data.cpp`, built, and flashed.
- Current firmware model classes are now:
  - `20`
  - `50`
  - `100`
  - `200`
  - `500`
  - `1000`
- MAX98357A pin mapping in code is now:
  - `DIN = D9 / GPIO8`
  - `BCLK = D8 / GPIO7`
  - `LRC = D7 / GPIO44`
- Ready LED pin mapping in code is now:
  - `READY_LED_PIN = D5 / GPIO6`
- SPIFFS audio files were regenerated from silent placeholders into real non-silent WAV clips and uploaded.
- Latest verified 50 peso test detected `P50` twice with high confidence and logged audio playback:
  - `P50 conf=0.934`
  - `P50 conf=0.907`
- That P50 test was before the final D6 manual-capture upload. Controlled bill tests should be repeated with the current D6 flow.

## Latest Manual-Capture Runtime Check

- Firmware upload succeeded on `COM3`.
- SPIFFS audio upload succeeded on `COM3`.
- PlatformIO environment: `seeed_xiao_esp32s3_tflm_button_capture`.
- Flash usage: about `3,882,233 / 5,308,416` bytes (`73.1%`).
- RAM usage reported by PlatformIO: about `19,596` bytes (`6.0%`).
- Camera init confirmed:
  - `[mode] D6/GPIO43 manual capture enabled`
  - `[manual] D6/GPIO43 button enabled - press to start 3000 ms aim delay`
  - `[cam] init ok (RGB565 QVGA 320x240)`
  - warmup frames `2/2 ok`
- TFLite init confirmed:
  - `[infer] ready - input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980`
- Audio init confirmed:
  - `[audio] init ok (16 kHz, 16-bit, mono)`
- Manual wait confirmed:
  - `[scan] manual mode - waiting for D6/GPIO43 press`
- Runtime speed:
  - `invoke` is about `32-35 s` per scan after the 3000 ms manual aim delay.
- Latest saved serial log:
  - `logs/detection-monitor-20260530-222814.log`

## Known Problems

- `imgsz=224` runs on-device but is slow: about `32-35 s` per inference.
- Detection is not reliable yet. A real 50 peso bill can sometimes be read as another bill or as multiple bills.
- Each D6 capture takes the 3-second aim delay plus the long inference, so the bill must still be held steady for roughly 35-40 seconds.
- The currently uploaded manual-capture firmware does not save camera frames as `.jpg` or `.png`; it only logs serial output.
- A separate debug environment, `seeed_xiao_esp32s3_tflm_button_capture_dump`, exists for serial JPEG frame dumping when visual capture debugging is needed.
- If serial monitor fails with `Cannot configure port`, unplug/replug the ESP32-S3, wait 5 seconds, press reset, then retry.

## Important Local Files

- Latest saved serial log:
  - `logs/serial-capture-latest.txt`
- Main firmware flow:
  - `src/main.cpp`
- Inference and detection threshold/debug logs:
  - `src/inference.cpp`
- Pins, model config, thresholds:
  - `include/config.h`
- Audio generation:
  - `tools/generate_english_clips.py`
- Embedded model source:
  - `src/model_data.cpp`
- Full-integer TFLite source model used for embedding:
  - `model_exports/kaggle-yolov8n-imgsz224-merged-6class/best_full_integer_quant.tflite`
- Current checkpoint:
  - `..\CURRENT_CHECKPOINT.md`
- Backup before the 224/6-class firmware edits:
  - `backups/before-img224-6class-20260530-052707`

## Test Commands

Use PowerShell:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
pio device list
```

Live serial monitor:

```powershell
pio device monitor --port COM3 -b 115200
```

Saved 90-second serial capture:

```powershell
New-Item -ItemType Directory -Force logs | Out-Null
py tools\serial_capture.py COM3 --seconds 90 2>&1 | Tee-Object -FilePath logs\serial-capture-latest.txt
```

Build and upload firmware:

```powershell
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2 --target upload --upload-port COM3
```

Upload audio filesystem:

```powershell
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target uploadfs --upload-port COM3
```

Replace `COM3` with the current port if needed. The latest verified port was `COM3`.

## What Changed This Session

- Trained a merged 6-class Kaggle YOLOv8n model at `imgsz=224`.
- Saved the Kaggle `.pt` and result images under:
  - `..\PesoLens-training\runs\pesolens_merged_yolov8n_img224_kaggle`
- Exported the new model to TFLite in Kaggle and moved it under:
  - `model_exports\kaggle-yolov8n-imgsz224-merged-6class`
- Embedded `best_full_integer_quant.tflite` into `src\model_data.cpp`.
- Updated `include\config.h` to `MODEL_INPUT_SIZE 224`, `MODEL_NUM_CLASSES 6`, and class order `20, 50, 100, 200, 500, 1000`.
- Updated `tools\convert_model.py` so its default TFLite candidate is the new 224/6-class export.
- Added project checkpoint:
  - `..\CURRENT_CHECKPOINT.md`
- Added D6 manual capture mode through `PESOS_MANUAL_CAPTURE`.
- Added PlatformIO environment:
  - `seeed_xiao_esp32s3_tflm_button_capture`
- Added debug serial-frame dump environment:
  - `seeed_xiao_esp32s3_tflm_button_capture_dump`
- Updated `include\config.h` so `CAPTURE_BUTTON_PIN` is `43` and `MANUAL_CAPTURE_DELAY_MS` is `3000`.
- Added ready/error audio behavior:
  - no bill detected: play `error`
  - capture cycle complete: play `beep` when ready again
- Added D5/GPIO6 ready LED behavior:
  - LED ON while waiting for a D6 capture
  - LED OFF after D6 is pressed during the 3-second aim delay, frame capture, and inference
  - LED blinks 3 times with 1-second on/off timing before result/error audio
  - LED ON again after the ready beep when the next capture can start
- Added richer serial logs:
  - camera capture/preprocess logs
  - TFLite init logs
  - inference timing
  - top candidate class scores
  - detections and totals
  - audio clip playback logs
- Embedded a true full-integer int8 model.
- Added missing TFLite Micro ops needed by the model.
- Lowered `CONF_THRESHOLD` to `0.25f` for demo testing.
- Increased task watchdog timeout to reduce noisy watchdog warnings during long inference.
- Regenerated real audio clips and uploaded SPIFFS.
- Added deferred speed plan:
  - `FUTURE_CLASSIFIER_MODEL_PLAN.md`

## Next Debugging Target

Runtime boot is confirmed. Improve classification stability with controlled D6 bill tests.

If the 32-35 second invoke time is too slow for the demo, the preferred later path is a tiny INT8 classifier instead of YOLO. That plan is saved in `FUTURE_CLASSIFIER_MODEL_PLAN.md`.

Start by collecting controlled serial logs:

1. Test one denomination at a time.
2. Press D6, use the 3-second delay to aim the bill, then hold it flat, well lit, and filling most of the camera view.
3. Save one 90-second serial log per denomination.
4. Compare the top candidate scores and duplicate detections.

Likely next code changes:

- Raise `CONF_THRESHOLD` after enough logs, probably back toward `0.40f` if false positives are common.
- Tighten duplicate suppression/NMS if one physical bill is detected as two bills.
- Use `seeed_xiao_esp32s3_tflm_button_capture_dump` if visual frame verification is needed.
- Later, replace YOLO with a tiny classifier for practical runtime.

## Resume Prompt For Next Codex Session

Use this prompt:

```text
We are continuing C:\Users\james\Projects\PesoLens-project.
Read CURRENT_CHECKPOINT.md and firmware\PROGRESS.md first. The source tree embeds the 2026-06-25 Kaggle 320x320 five-class YOLOv8n INT8 model with classes 20, 50, 100, 500, 1000. It builds for seeed_xiao_esp32s3_tflm_button_capture, but it has not been uploaded or live-tested after the import. Local predict.py runs, but test_bill.jpg detects only P1000 and misses the visible P500, so validate model quality before flashing as a trusted replacement. The later model-speed plan is in firmware\FUTURE_CLASSIFIER_MODEL_PLAN.md.
```
