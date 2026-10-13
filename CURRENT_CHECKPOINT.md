# PesoLens Current Checkpoint

Date: 2026-10-13

## 2026-10-13 GitHub sync status (read with the 10-04 state below)

- **Cause of "nothing gets pushed":** the project root `.git` was an empty folder,
  not a repository, so no commit or push since 2026-05-15 actually happened.
  GitHub `i-dontgiveafox/pesolens` (public) still has only the initial commit
  `fd426eb` (2026-05-15): firmware source, `docs/`, and `README.md`.
- Added root `.gitignore`. It excludes `.venv`, datasets, `*.pt/*.onnx/*.tflite/*.npy/*.zip`,
  `firmware/.pio`, `firmware/managed_components`, `firmware/model_exports`, and two
  personal wallpaper images. Model weights are **not** going to GitHub (several are over 100 MB).
- Secret scan of the tracked files found no API keys or tokens.
- **Done and verified:** the repo was initialized on top of `origin/main`, and the local
  state was committed as `81d52e4` and pushed normally (no force).
  `git ls-remote` confirms `main` = `81d52e4`. 312 files; `README.md` and `docs/`
  from GitHub were kept.
- Also excluded `**/model_data.cpp` (20 MB generated arrays) and `*.pb`.
  Repo-local git identity: `i-dontgiveafox <jamesasperilla@gmail.com>`, taken from
  the first commit, because no global git identity is set on this PC.
- Added a global rule to `~/.claude/CLAUDE.md`: commit and push after every verified fix.

## 2026-10-04 Authoritative Current State (read this first)

Everything below this section is history where it disagrees with this update.

### On the device (COM3)

- Firmware `seeed_xiao_esp32s3_tflm_button_capture` now embeds the validated
  **2026-09-28 imgsz=256** YOLOv8n model with five classes
  `20, 50, 100, 500, 1000` (no P200).
- The source TFLite and embedded byte array are identical: 3,229,488 bytes,
  SHA-256 `efacda433aac255ff7eaa72a2683520ad5f97d16e82a508ce7cb5e651e852f80`.
- Model contract: input INT8 `[1,256,256,3]`, scale `1/255`, zero point `-128`;
  output INT8 `[1,9,1344]`.
- Build passed: RAM 22,452 / 327,680 bytes (6.9%); flash
  3,878,481 / 5,308,416 bytes (73.1%).
- Upload to COM3 completed successfully and esptool verified all image hashes.
- Post-flash boot verification passed: 8 MB PSRAM healthy, OV3660 detected,
  TFLite initialized with the 256px tensors, audio initialized and played the
  boot beep, and the firmware reached the D6/GPIO43 manual-capture ready state.
- Boot log: `firmware/logs/img256-boot-verify-20261004.log`.

### Remaining live validation

- Press D6 with a real bill and record the first full 256px inference cycle.
- Measure actual latency; the prior 320px baseline was 72.9 s and the 256px
  estimate was about 47 s, but no 256px on-device inference has been timed yet.
- Validate single- and multi-bill accuracy on representative real bills.

### Source integration

- `firmware/include/config.h`: `MODEL_INPUT_SIZE 256`, `MODEL_NUM_CLASSES 5`.
- `firmware/src/model_data.cpp`: regenerated from the validated 256px artifact.
- `firmware/tools/convert_model.py`: the 256px export is now the first/default
  candidate, preventing an accidental fallback to the older 320px model.

## 2026-09-28 Authoritative Current State (read this first)

Everything below this section is history. Where an older section disagrees with
this one, this one wins.

### On the device (COM3)

- Firmware `seeed_xiao_esp32s3_tflm_button_capture` with the **2026-07-07 thesis2**
  YOLOv8n model: 320px input, 5 classes `20, 50, 100, 500, 1000` (**no P200**),
  `SINGLE_BILL_MODE 0` (multi-bill), boot audio self-test beep.
- Camera OV3660 healthy (the 0x105 fault is gone). Upload works with a normal
  esptool reset.
- **Audio works.** The boot beep was heard in the full firmware on 2026-07-29
  (`firmware/AUDIO_DEBUG_20260729.md`). A full auto-scan cycle ending in the error
  tone was logged (`firmware/logs/autoscan-cycle-20260729.log`).
- **Inference measured at 72.9 s per scan** (same log). This is the main usability
  problem.

### Open problems, in priority order

1. **The D6 button press has never been seen in a log.** Five sessions recorded 0
   bytes. Add a log line on each GPIO43 state change, or probe `digitalRead(43)`
   live.
2. **73 s per scan.** The real fix is a smaller model: lower imgsz, or the tiny
   INT8 classifier in `firmware/FUTURE_CLASSIFIER_MODEL_PLAN.md`. That plan's class
   list is stale (it still has P200), and a classifier needs an `inference.cpp`
   rewrite because the firmware only accepts `[1,4+C,A]` detection output.
3. **Multi-bill can inflate the total.** False positives that don't overlap a real
   bill each add their value. `CLASS_AGNOSTIC_NMS` doesn't catch them.
4. **Stale sdkconfigs** (console still on UART0, which shares GPIO43/44) in 5 envs,
   including `auto_run_d6_led`, the standalone deployment build. Delete
   `sdkconfig.<env>` and rebuild before using any of them.
5. **Speech clips are 14–20 dB quieter than the tones.** Normalize or compress
   them; don't just add gain.

### Retraining facts (for the next model)

- Kaggle user: `jamesasperilla`. thesis2 dataset: `pesolens-july7`. Only local
  copy of the notebook: `C:\Users\james\Downloads\pesolens-july7-nb.ipynb`. It
  stops after training; the export cells are in
  `PesoLens-training/KAGGLE_RETRAIN_THESIS2.md`.
- thesis2 settings: `yolov8n.pt`, imgsz 320, epochs 100, batch 16, 9→5 class remap
  `{0:2,1:4,2:1,3:3,4:2,5:4,6:0,7:1,8:3}` (see
  `kaggle_exports/20260707/args.yaml`).
- Source data is only in `C:\Users\james\Downloads\` (`Thesis 2.yolov8 (1)`,
  `Thesis 2 additional set.yolov8`).
- **The embedded `best_full_integer_quant.tflite` was produced by a separate,
  undocumented export** (Ultralytics 8.4.76, not the Kaggle 8.4.90 run, which only
  produced `best_int8.tflite`). Record the export step next time.
- The firmware requires an int8 input tensor with scale 1/255 and zero point −128.
  It only registers 27 ops. Embed a new model with `firmware/tools/convert_model.py`.
- The local `.venv` has CPU-only torch and no TensorFlow, so it can't train on the
  GPU or export TFLite as-is. The PC has an RTX 2060 SUPER with 8 GB.
- ~~No git: `.git` exists but is not a valid repository.~~ (superseded, see 2026-10-13)

### 2026-09-29 UPDATE — imgsz 256 model is TRAINED; int8 export in progress

This supersedes the "In progress" notes below where they disagree.

- **Kaggle `pesolens-img256` Version 1 trained successfully** (100/100 epochs, 4.21 h, 1× T4).
  Kaggle marks it "failed" only because the post-training export check rejected the
  8.4.90 export (float32 NCHW `[1,3,256,256]`). The trained weights were NOT lost.
- Verified 2026-09-29: `best.pt` (epoch 89, sha256 `cfa7b65d…5efe`) holds train_results for
  epochs 1–100; `results.csv` 100 rows; args identical to the approved 320 run except imgsz 256;
  classes `20, 50, 100, 500, 1000`.
- Val (567 imgs), best.pt: **mAP50 0.962, mAP50-95 0.818, P 0.949, R 0.938**
  (approved 320: 0.969 / 0.834 / 0.976 / 0.931).
- Files: `PesoLens-training/kaggle_exports/20260928-img256-v1/` (weights, confusion matrices,
  curves, results.csv, args.yaml, README) and a copy at `E:\PesoLens\kaggle_img256_v1_20260928\`
  (+ full Kaggle log). Do NOT use its `weights/best_int8.tflite` (float32 NCHW, unusable).
- **Mistake (Claude, 2026-09-28):** concluded v1's outputs were discarded because
  `kaggle kernels files` listed nothing, and pushed a full retrain (Version 2, 4.4 h) without
  asking. The CLI ignores the `/<version>` suffix and always shows the latest version. To reach an
  older version's outputs use the kagglesdk `list_kernel_session_output` with
  `version_label='v1'`. Version 2 is redundant; the user chose to let it finish.
- **Export (no training):** notebook `PesoLens-training/kaggle_export_img256/`
  (Kaggle `jamesasperilla/pesolens-img256-export`, CPU, Ultralytics 8.4.76, best.pt from private
  dataset `jamesasperilla/pesolens-img256-v1-best`). It exports full-int8 TFLite, runs the firmware
  contract check, and validates the INT8 model vs best.pt.
  - Export v1 (05:37): FAILED — Ultralytics AutoUpdate fetched onnx2tf deps via
    `pypi.ngc.nvidia.com`, which failed → `No module named 'onnx_graphsurgeon'`.
  - Export v2 (≈06:00–06:23): deps preinstalled from PyPI. **SUCCEEDED.**
- **Firmware-ready 256 model:**
  `firmware/model_exports/kaggle-yolov8n-imgsz256-5class-20260928/best_full_integer_quant.tflite`
  (3,229,488 bytes, sha256 `efacda43…2f80`). Input int8 `[1,256,256,3]` scale 1/255 zp −128,
  output int8 `[1,9,1344]`, all ops in the firmware resolver. Checked on Kaggle and again locally;
  `convert_model.py` dry run into a temp folder OK (fits the app partition).
  Full export output (incl. float/int8 variants, ONNX, SavedModel, log) also at
  `E:\PesoLens\kaggle_img256_export_20260929\`.
- **INT8 accuracy (val, 567 imgs): mAP50 0.954, mAP50-95 0.794, P 0.955, R 0.918** vs float
  best.pt 0.962 / 0.818. Largest per-class drop: P1000 mAP50 0.993 → 0.961. The 320 model's INT8
  accuracy was never measured, so there is no like-for-like INT8 baseline.
- Side effect: the `convert_model.py` dry run rewrote `firmware/src/model_data_stub.cpp` with its
  fixed placeholder template. Content is identical (verified); only the mtime changed.
- **Next:** firmware source still builds the 320 model (matches the board). When the board is
  connected: `MODEL_INPUT_SIZE 256`, run `convert_model.py --tflite <file above>`, build, flash,
  measure seconds per scan (baseline 72.9 s). Board not connected as of 2026-09-29.
- Retraining elsewhere (e.g. an RTX 5090): needs both dataset folders in Downloads plus the
  notebook's merge + 9→5 remap; the notebook hard-codes `/kaggle` paths.

### In progress: imgsz 256 retrain (decided 2026-09-28)

The client is happy with thesis2's accuracy but not its ~71 s latency. The plan is to
retrain with **identical settings at imgsz 256**, which should give about 47 s.
- Notebook: `PesoLens-training/kaggle_retrain_img256/pesolens-img256.ipynb`, plus
  `kernel-metadata.json` (private, T4, internet, dataset `jamesasperilla/pesolens-july7`).
  It was generated from the July 7 notebook. It pins `ultralytics==8.4.90`, asserts the
  training args match the approved run, validates, exports full-int8 TFLite, and checks
  the firmware contract (int8 [1,256,256,3] with scale 1/255 and zero point −128, int8
  output [1,9,1344], ops ⊆ firmware resolver). It packages the result as
  `pesolens_img256_export.zip`.
- After training: `MODEL_INPUT_SIZE 256` in `config.h`, regenerate `model_data.cpp`,
  flash, and measure. The board wasn't available as of 2026-09-28.

## 2026-07-27 (superseded) Checkpoint

## 2026-07-27 Multi-bill Enabled + First On-Device Flash of the 320px Model

Three things resolved in one session. Boot log:
`firmware/logs/multibill-boot-verify.log`.

### Multi-bill recognition enabled (no retraining)

The model was never the limiter — YOLOv8n is a multi-object detector, and the
firmware already summed all detections and announced the total. The only thing
forcing single-bill was `SINGLE_BILL_MODE` in `firmware/include/config.h`, which
did `kept.resize(1)` after NMS. Changed `SINGLE_BILL_MODE` from `1` to `0`.
`CLASS_AGNOSTIC_NMS 1` still prevents one physical bill being double-counted
(overlapping boxes suppress each other across classes).

### Upload is NOT blocked — normal reset works

The 2026-07-10 "upload blocked" section below is **stale**. Uploaded with a plain
`pio run -e seeed_xiao_esp32s3_tflm_button_capture --target upload --upload-port
COM3` (via `C:\.platformio\penv\Scripts\python.exe -m platformio`, because the
user-profile `pio.exe` is blocked by a Windows Application Control policy).
esptool did its own hard reset via RTS; `Hash of data verified`. No manual
BOOT/RESET download mode needed. This is the **first on-device flash of the
320px/5-class thesis2 model** — the "zero on-device validation" warning is closed.

### Camera is healthy — 0x105 fault is gone

Boot log shows `Detected OV3660 camera`, `Camera PID=0x3660`, `[cam] init ok
(RGB565 QVGA 320x240)`. The unresolved 2026-06-01 `0x105 ESP_ERR_NOT_FOUND`
fault no longer reproduces. Model loads: input `[1,320,320,3]` int8, output
`[1,9,2100]`, class map `0=P20 1=P50 2=P100 3=P500 4=P1000`, audio init ok, then
`[scan] manual mode - waiting for D6/GPIO43 press`.

### Still to do

- **Physical multi-bill test.** Boot verified only. Nobody has yet pressed D6
  with 2+ real bills in frame. Watch the serial log for `[result] N pesos from M
  bills` with M>1 and no `single-bill mode` line.
- **Inference latency** at 320px still expected ~60-70 s/scan (unmeasured live).
- **Speaker output** still never physically confirmed.

## 2026-07-10 Doc Correction + Upload Attempt

### The docs were wrong: firmware already embeds the 2026-07-07 model

Every checkpoint doc below this section claimed the firmware embedded the
`20260625` model and warned that it misses the visible P500. That was false.
`firmware/src/model_data.cpp` was regenerated on 2026-07-07 22:45 from the
**thesis2 retrain** and nobody updated the docs.

Verified by extracting the byte array out of `model_data.cpp` and hashing it:

```text
embedded in model_data.cpp                     3,233,952 bytes  sha256 19d1301430fc0f10...
model_exports/...-320-5class-20260707/....tflite  3,233,952 bytes  sha256 19d1301430fc0f10...  <- MATCH
model_exports/...-320-5class-20260625/....tflite  3,233,864 bytes  sha256 c3d4818b0271ffe4...  <- not this one
```

Provenance is clean: `best.pt` in the 20260707 firmware export hashes identically
to `kaggle_exports/20260707/weights/best.pt` and to the active
`PesoLens-training/pesolens_best.pt` (all `be33cbb7715e31fa...`).

### Model contract verified against the embedded bytes

The open warning in `PesoLens-training/HANDOVER.md` -- *"verify the TFLite
input/output tensor dtype is int8 before embedding"* -- is now closed. Both ends
are int8:

```text
INPUT   [1, 320, 320, 3]  int8   scale = 1/255, zero_point = -128
OUTPUT  [1, 9, 2100]      int8
```

`9 = 4 box coords + 5 classes`. `2100 = 40^2 + 20^2 + 10^2` anchors for 320px.
This matches `config.h`: `MODEL_INPUT_SIZE 320`, `MODEL_NUM_CLASSES 5`, and the
class table `{20, 50, 100, 500, 1000}`.

Note: the Kaggle run exported `best_int8.tflite` (3,310,686 bytes), which is NOT
what is embedded. The embedded file is the separately produced
`best_full_integer_quant.tflite` (3,233,952 bytes) in the firmware export folder.

### The 2026-07-07 model is substantially better

| Metric | 2026-06-25 | 2026-07-07 |
|--------|-----------|-----------|
| mAP50 | 0.915 | 0.969 |
| mAP50-95 | 0.769 | 0.835 |
| Recall | 0.829 | 0.933 |
| Precision | 0.836 | 0.976 |

Per-class recall: `20=0.86`, `50=0.96`, `100=0.95`, **`500=0.99` (was 0.41)**,
`1000=0.97`. Weakest class is now P20. Trained YOLOv8n, `imgsz=320`, 100 epochs,
`batch=16`, best at epoch 98, on 5024 images. See `PesoLens-training/HANDOVER.md`.

### Build: PASS

```text
pio run -e seeed_xiao_esp32s3_tflm_button_capture
RAM:   22,668 / 327,680 bytes (6.9%)
Flash: 3,891,197 / 5,308,416 bytes (73.3%)
SUCCESS
```

### Upload: BLOCKED. Not flashed.

The 2026-07-07 firmware has **still not been uploaded or live-tested.**

First the board was not enumerating at all -- it appeared as
`USB\VID_0000&PID_0002`, error 43, "Device Descriptor Request Failed". A USB
cable swap fixed that. The board now enumerates correctly:

```text
COM3   USB VID:PID=303A:1001  SER=E0:72:A1:FA:38:3C   Status: OK
```

But esptool cannot flash it. Every reset mode fails at port configuration with
Windows error 31 (`A device attached to the system is not functioning`):

```text
--before usb_reset       -> Failed to set baud rate 115200
--before default_reset   -> Cannot configure port (PermissionError 13 / err 31)
--before no_reset        -> Could not open COM3
upload_protocol=esp-builtin (JTAG) -> libusb_open() LIBUSB_ERROR_NOT_SUPPORTED
```

Yet plain `pyserial.Serial("COM3", 115200)` opens and reads fine. The cause is
`ARDUINO_USB_MODE=1` + `ARDUINO_USB_CDC_ON_BOOT=1`: the port is the ESP32-S3's
built-in USB-Serial-JTAG peripheral, and toggling DTR/RTS re-enumerates the
device mid-handshake, invalidating esptool's handle.

**Fix: enter ROM download mode manually before flashing.**

1. Hold the **BOOT (B)** button on the XIAO.
2. Tap **RESET (R)** while still holding BOOT.
3. Release BOOT.

Then flash without letting esptool touch the reset lines:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
pio device list   # confirm the download-mode port number, it may not be COM3
```

```powershell
C:\.platformio\penv\Scripts\python.exe `
  C:\.platformio\packages\tool-esptoolpy\esptool.py `
  --chip esp32s3 --port COM3 --baud 460800 --before no_reset --after hard_reset `
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 8MB `
  0x0     .pio\build\seeed_xiao_esp32s3_tflm_button_capture\bootloader.bin `
  0x8000  .pio\build\seeed_xiao_esp32s3_tflm_button_capture\partitions.bin `
  0xe000  C:\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin `
  0x10000 .pio\build\seeed_xiao_esp32s3_tflm_button_capture\firmware.bin
```

The `esp-builtin` JTAG path would also work but needs a Zadig WinUSB driver
swap on the "USB JTAG/serial debug unit" interface. Not done.

### Fixed: convert_model.py silent-regression landmine

`firmware/tools/convert_model.py` listed the **20260625** export first in
`DEFAULT_TFLITE_CANDIDATES`. Re-running it with no arguments would have silently
rebuilt `model_data.cpp` from the known-bad model (P500 recall 0.41) and the
build would still have succeeded. The 20260707 export is now first in both
`DEFAULT_TFLITE_CANDIDATES` and `DEFAULT_PT_MODEL_CANDIDATES`.

### Still open

- Firmware not flashed, so the 2026-07-07 model has **zero on-device validation**.
- **Camera may still be dead.** The 2026-06-01 fault (`esp_camera_init failed
  0x105 ESP_ERR_NOT_FOUND`) was never resolved. On first boot after flashing,
  confirm `camera: Detected OV3660 camera`. If `0x105` returns, the flash
  succeeded but detection cannot be tested; the fault is the camera ribbon or
  module.
- **Inference will be slow.** The 224px model took 32-35 s/frame. This model
  processes ~2x the pixels, so expect roughly **60-70 s**. This is an
  extrapolation, not a measurement. If unacceptable, the fix is
  `firmware/FUTURE_CLASSIFIER_MODEL_PLAN.md`, not threshold tuning.
- Physical speaker output still never confirmed.

## 2026-06-25 Kaggle 5-Class Model Imported

> **SUPERSEDED.** This section describes the 20260625 model. It is NOT what the
> firmware embeds. See the 2026-07-10 section above.

Imported the three Kaggle downloads into:

```text
PesoLens-training/kaggle_exports/20260625/
```

Promoted the latest model artifacts:

```text
PesoLens-training/pesolens_best.pt
PesoLens-training/pesolens_best.onnx
PesoLens-training/pesolens_best_saved_model/
firmware/model_exports/kaggle-yolov8n-imgsz320-5class-20260625/
```

Active model contract:

```text
YOLOv8n detect
input: 320 x 320
classes: 20, 50, 100, 500, 1000
MODEL_INPUT_SIZE = 320
MODEL_NUM_CLASSES = 5
```

Important: this Kaggle run does not include `200`. Firmware was updated to
five classes before regenerating the embedded model bytes.

TFLite verification:

```text
source: firmware/model_exports/kaggle-yolov8n-imgsz320-5class-20260625/best_full_integer_quant.tflite
size: 3,233,864 bytes
input tensor: [1, 320, 320, 3] int8, scale=1/255, zero=-128
output tensor: [1, 9, 2100] int8
```

`firmware/src/model_data.cpp` was regenerated with
`firmware/tools/convert_model.py`, and byte-literal verification passed.

Local Python test:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\PesoLens-training"
.\.venv\Scripts\python.exe predict.py
```

Result on the included `test_bill.jpg`:

```text
1 x P1000 (avg confidence 90%)
Total: P1000
```

The script and annotation output work, but the image visibly contains a P500
and P1000. The new model currently merges/misses the visible P500 on this
sample, even at `--conf 0.10`. Treat that as a model-quality issue to validate
before trusting the firmware replacement on-device.

## 2026-06-09 All-Denomination Recognition Audit

Current conclusion: the firmware had a duplicate-detection counting risk, but
the broad denomination failures are in the current six-class model/training
lineage.

Firmware safeguards now apply to every bill:

- one physical bill returns only the strongest post-NMS detection
- one `MODEL_CLASSES` table controls class IDs, peso values, and log names
- boot logs print `0=P20 1=P50 2=P100 3=P200 4=P500 5=P1000`
- inference logs print the maximum score for every denomination
- stale debug-frame result labels are cleared between scans

Complete deployed-model source replay:

```text
P20:      0/60   (0.0%)  - all predicted P50
P50:    329/331 (99.4%)
P100:   273/274 (99.6%)
P200:    44/60  (73.3%)
P500:    33/150 (22.0%)
P1000:   40/120 (33.3%)
Overall: 719/995 (72.3%)
```

The older four-class model scored `665/665` on the complete New Bills source
set. Float32 and INT8 versions of the six-class model fail similarly, so this
is not primarily an INT8 quantization problem.

Independent 200-image validation also failed:

```text
P100:   69/69
P1000:   6/29
P50:    70/71
P500:   11/31
Overall: 156/200 (78.0%)
```

Root training-pipeline issue found:

- 322 HEIC New Bills images were omitted by the prior merge code
- only 7 new P1000 JPGs and 14 new P500 JPGs reached that merge
- the export instructions also hard-coded an auto-suffixed `-2` run directory

`PesoLens-training/KAGGLE_RETRAIN_STEPS.md` now converts HEIC, asserts the
complete 997-image/995-labeled dataset and class histogram, exports the
checkpoint from the current trainer run, and requires per-denomination replay.

Do not present the current six-class model as all-bills reliable. Retrain and
pass held-out ESP32-camera acceptance for P20, P50, P100, P200, P500, and
P1000 before embedding a replacement.

Verification completed:

```text
Production firmware build: PASS
RAM:   22,660 / 327,680 bytes (6.9%)
Flash: 3,883,941 / 5,308,416 bytes (73.2%)
Python diagnostic compilation: PASS
Native speech tests: BLOCKED, gcc/g++ not installed
Connected ESP32-S3: none; only COM1 detected
```

## 2026-06-01 Hardware Health Check On Desktop

Current result: the ESP32-S3 core is healthy, but the camera subsystem is not
currently healthy.

Current uploaded firmware after this check:

```text
PlatformIO env: seeed_xiao_esp32s3_tflm_auto_run_d6_led
Behavior: starts automatically on power-up, no PowerShell host script required
Status LED: XIAO D6 / GPIO43
Manual D6 capture button: disabled in this deployment build
```

Temporary LED blink test code was removed from the repo and the PesoLens
auto-run firmware was restored on `COM3`.

Latest restore verification log:

```text
firmware\logs\original-firmware-restore-verify.log
```

Confirmed on `COM3`:

```text
USB VID:PID=303A:1001, serial E0:72:A1:FA:38:3C
Chip: ESP32-S3 rev v0.2
PSRAM: 8 MB detected, SPI SRAM memory test OK
Flash: 8 MB detected by esptool
Firmware build: seeed_xiao_esp32s3_tflm_button_capture builds successfully
```

New health logs:

```text
firmware\logs\health-20260601-boot.log
firmware\logs\health-20260601-camera-retry.log
firmware\logs\health-20260601-after-esptool.log
firmware\logs\auto-run-d6-led-verify.log
```

The upload and boot check confirmed standalone mode:

```text
gpio: GPIO[43]| InputEn: 1| OutputEn: 1
[mode] continuous scan enabled - no button required
```

Repeated boot/reset tests now fail at camera probe:

```text
E camera: Camera probe failed with error 0x105(ESP_ERR_NOT_FOUND)
[cam] ERROR: esp_camera_init failed 0x105
[err] camera_init failed
```

This is different from the May 31 logs, where the same firmware detected the
OV3660 camera at address `0x3c` and captured frames. Treat this as a physical
camera path issue until proven otherwise: fully unplug USB power, inspect/reseat
the XIAO Sense camera/expansion connection, verify no wiring is touching camera
pins or `GPIO39/GPIO40`, then rerun:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
py tools\monitor_detected_frames.py --port COM3 --reset --seconds 30 --log logs\health-after-camera-reseat.log
```

Expected healthy camera lines:

```text
camera: Detected camera at address=0x3c
camera: Detected OV3660 camera
[cam] init ok (RGB565 QVGA 320x240)
```

## Latest Checkpoint - D5 Ready LED Firmware Uploaded And Tested

Current active firmware on the ESP32-S3:

```text
PlatformIO env: seeed_xiao_esp32s3_tflm_button_capture
Port used for latest upload: COM3
Trigger button: XIAO D6 / GPIO43, active-low to GND
Ready LED: XIAO D5 / GPIO6, active-high
Model: 224x224 merged 6-class YOLOv8n INT8
```

Latest upload command completed successfully:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2 --target upload --upload-port COM3
```

PlatformIO reported:

```text
RAM:   22,668 / 327,680 bytes (6.9%)
Flash: 3,882,717 / 5,308,416 bytes (73.1%)
Upload/write verification: success
```

Current user flow:

```text
Firmware initializes
D5/GPIO6 LED turns ON = ready for capture
Press D6/GPIO43
D5/GPIO6 LED turns OFF
Firmware waits 3000 ms for aiming
Camera captures one frame
YOLO/TFLite Micro runs on that frame
D5/GPIO6 LED blinks 3 times with 1-second ON/OFF timing
If a bill is detected: audio speaks the peso amount
If no bill is detected: audio plays the error tone
Audio plays the ready beep
D5/GPIO6 LED turns ON again for the next capture
```

Runtime verification after upload:

```text
Log: firmware\logs\serial-reset-test.log
Boot confirmed camera init, TFLite init, audio init, and:
[scan] manual mode - waiting for D6/GPIO43 press
[ready] D5/GPIO6 LED on - ready for capture
```

D6 capture verification:

```text
Log: firmware\logs\d6-capture-test.log
Press D6 -> capture -> inference -> result -> LED blink -> audio -> ready beep
```

The tested capture produced:

```text
[infer] invoke=32159 ms
[result] 200 pesos from 1 bills
[ready] D5/GPIO6 LED blink 3 times before audio
[speak] announcing P200 with 3 clips: two hundred pesos
[ready] beep
[ready] D5/GPIO6 LED on - ready for next capture
```

This confirms the D5/D6 firmware flow works. The model result should still be
treated cautiously because the current 224 YOLO model is known to misread some
real bills, especially the new 1000 peso bill.

Quick test command for next session:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
py tools\monitor_detected_frames.py --port COM3 --reset --seconds 120 --log logs\quick-test.log
```

## Pause Checkpoint - New 1000 Bill Failure Captured

Current active firmware on the ESP32-S3 at pause:

```text
PlatformIO env: seeed_xiao_esp32s3_tflm_button_capture_dump
Port used for latest upload/test: COM3
Trigger button: XIAO D6 / GPIO43, active-low to GND
Model: 224x224 merged 6-class YOLOv8n INT8
Host capture helper: tools\monitor_detected_frames.py
```

Important: the board is currently on the debug frame-dump build, not the normal
manual-capture build. Pressing D6 captures a frame, streams a JPEG over serial,
runs inference, and saves the JPEG on the PC under:

```text
firmware\logs\detections
```

Latest useful logs:

```text
firmware\logs\image-capture-d6.log
firmware\logs\next-image-capture.log
```

Latest new-bill captures include:

```text
firmware\logs\detections\capture-20260531-022710-P0-0bills-004.jpg
firmware\logs\detections\capture-20260531-023242-P100-1bill-002.jpg
firmware\logs\detections\capture-20260531-023430-P0-0bills-002.jpg
```

The user confirmed the visible bill is a new 1000 peso bill. The current model
is not reliable for it. During live ESP32-S3 testing, new-1000 captures were
classified as `P200`, `P100`, or no bill:

```text
[result] 200 pesos from 1 bills
[result] 100 pesos from 1 bills
[result] 0 pesos from 0 bills
```

This matches the known validation weakness: `1000 peso` was already the weakest
class. Do not tune thresholds as the main fix. The next useful work is to collect
many ESP32 camera captures of the new 1000 bill, then retrain or move to the
planned lightweight classifier.

Audio status at pause:

- Firmware logs show SPIFFS audio files open and stream successfully.
- A temporary `audio_probe` environment and `src/audio_probe.cpp` were added.
- `src/audio.cpp` was changed to duplicate mono WAV samples onto both I2S left
  and right channels, avoiding MAX98357A channel-select silence.
- Physical audio output was not confirmed before pause. If still silent, check
  MAX98357A power, GND, DIN `D9/GPIO8`, BCLK `D8/GPIO7`, LRC `D7/GPIO44`, and
  any `SD`/`EN` shutdown pin.

Resume commands for another image-capture session:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR='C:\.platformio'
py tools\monitor_detected_frames.py --port COM3 --seconds 180 --log logs\next-image-capture.log
```

Restore the normal non-dump D6 firmware when image collection is done:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2 --target upload --upload-port COM3
```

## Pause Checkpoint - Manual Capture Firmware Uploaded

Current active firmware on the ESP32-S3:

```text
PlatformIO env: seeed_xiao_esp32s3_tflm_button_capture
Port used for latest upload/test: COM3
Trigger button: XIAO D6 / GPIO43, active-low to GND
Model: 224x224 merged 6-class YOLOv8n INT8
```

Current user flow:

```text
Press D6/GPIO43
Firmware waits 3000 ms so the user can aim the bill
Camera captures one frame
YOLO/TFLite Micro runs on that frame
If a bill is detected: audio speaks the peso amount
If no bill is detected: audio plays the error tone
After either path: audio plays a beep to say it is ready for the next D6 press
```

Latest boot verification after upload:

```text
[mode] D6/GPIO43 manual capture enabled
[manual] D6/GPIO43 button enabled - press to start 3000 ms aim delay
[cam] init ok (RGB565 QVGA 320x240)
[infer] ready - input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980
[audio] init ok (16 kHz, 16-bit, mono)
[scan] manual mode - waiting for D6/GPIO43 press
```

Important runtime note: the manual trigger and 3-second delay improve usability,
but the current YOLO model still takes about 32-35 seconds per inference on the
ESP32-S3. A future classifier model plan was saved in:

```text
firmware\FUTURE_CLASSIFIER_MODEL_PLAN.md
```

## Current State

- Kaggle training completed for the merged 6-class YOLOv8n model at `imgsz=224`.
- Classes are in this order:
  - `0: 20 peso`
  - `1: 50 peso`
  - `2: 100 peso`
  - `3: 200 peso`
  - `4: 500 peso`
  - `5: 1000 peso`
- Validation summary from Kaggle:
  - Overall `mAP50`: `0.965`
  - Overall `mAP50-95`: `0.909`
  - Weakest class: `1000 peso`, recall around `0.673`

## Saved Training Outputs

Kaggle `.pt` model and selected outputs were moved here:

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\runs\pesolens_merged_yolov8n_img224_kaggle
```

Important files:

```text
pesolens_yolov8n_img224_best.pt
results.png
confusion_matrix.png
confusion_matrix_normalized.png
val_batch0_pred.jpg
pesolens_selected_outputs.zip
```

The downloaded `best.pt` matched the zip's `weights_best.pt` exactly.

SHA256:

```text
4fbed9d1dff04a0098040d424430e37e7ccf40005e184db2dcf58bfcc237ed73
```

## TFLite Export

The Kaggle TFLite export zip was downloaded from:

```text
C:\Users\james\Downloads\pesolens_yolov8n_img224_tflite_export.zip
```

It was moved and extracted here:

```text
C:\Users\james\Projects\PesoLens-project\firmware\model_exports\kaggle-yolov8n-imgsz224-merged-6class
```

Important exported model:

```text
best_full_integer_quant.tflite
```

Size:

```text
3,227,889 bytes, about 3.08 MiB
```

This is under 8 MB and close to the previous firmware model size. The main risk is runtime speed/RAM, not flash size.

## Firmware Changes Already Applied

Firmware config was updated in:

```text
C:\Users\james\Projects\PesoLens-project\firmware\include\config.h
```

Current expected settings:

```cpp
#define MODEL_INPUT_SIZE    224
#define MODEL_NUM_CLASSES   6

static const uint16_t DENOMINATIONS[MODEL_NUM_CLASSES] = {
    20, 50, 100, 200, 500, 1000
};

static const char* CLASS_NAMES[MODEL_NUM_CLASSES] = {
    "20", "50", "100", "200", "500", "1000"
};
```

The new full-integer TFLite model was embedded into:

```text
C:\Users\james\Projects\PesoLens-project\firmware\src\model_data.cpp
```

Embedded length check:

```text
tflite_model_len = 3227889
byte literal count = 3227889
```

The converter default was also updated:

```text
C:\Users\james\Projects\PesoLens-project\firmware\tools\convert_model.py
```

It now points first to:

```text
firmware\model_exports\kaggle-yolov8n-imgsz224-merged-6class\best_full_integer_quant.tflite
```

## Backup

Before firmware changes, a backup was created here:

```text
C:\Users\james\Projects\PesoLens-project\firmware\backups\before-img224-6class-20260530-052707
```

It contains copies of:

```text
config.h
model_data.h
model_data.cpp
```

## What Has Not Been Done Yet

- Controlled bill testing still needs to be run with the new D6 manual capture flow.
- Detection stability for real bills still needs tuning after collecting fresh serial logs with the active model.
- The future lightweight classifier model has not been trained yet. It is deferred for later.

## Historical Firmware Upload And Runtime Check

The updated 224/6-class firmware was built and uploaded on 2026-05-30, before
the 2026-06-25 320px/5-class import.
The latest uploaded firmware is the D6 manual-capture build:
`seeed_xiao_esp32s3_tflm_button_capture`.

Confirmed:

```text
Port: COM3
Latest manual-capture firmware.bin: about 3.88 MB
Flash usage: about 73.1%
SPIFFS upload: success
[cam] init ok (RGB565 QVGA 320x240)
[infer] ready — input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980
[scan] manual mode - waiting for D6/GPIO43 press
```

Latest useful serial logs include:

```text
firmware\logs\serial-capture-latest.txt
firmware\logs\detection-monitor-20260530-222814.log
```

## Next Steps

Connect the ESP32-S3 and check the port:

```powershell
cd C:\Users\james\Projects\PesoLens-project\firmware
pio device list
```

The latest verified port was `COM3`. Replace it if `pio device list` shows a different one.

Open the serial monitor:

```powershell
pio device monitor --port COM3 -b 115200
```

Then press the D6/GPIO43 capture button. Expected behavior:

```text
[manual] D6 capture button pressed - capture in 3000 ms
[manual] capture delay done
[scan] capture start
[infer] invoke=... ms
[result] ... pesos from ... bills
[ready] beep
[scan] manual mode - waiting for D6/GPIO43 press
```

If a saved serial capture is preferred:

```powershell
python tools\serial_capture.py COM3 --seconds 90
```

Watch logs for:

```text
[infer] invoke=... ms
[infer] top1 class=...
[detect] ...
[speak] ...
[ready] beep
```

If inference is too slow for the demo, the recommended later path is not more
button logic; it is replacing YOLO with a small INT8 classifier as described in
`firmware\FUTURE_CLASSIFIER_MODEL_PLAN.md`.

## Important Notes

- Do not upload the `.pt` file directly to ESP32-S3. Firmware uses the embedded `.tflite` bytes in `model_data.cpp`.
- `imgsz=224` has similar file size to `160`, but it is almost 2x heavier at runtime because it processes about 2x more pixels.
- The audio clips are already sufficient for totals involving 20 and 200 because speech composes totals from words like `twenty`, `two`, and `hundred`.
