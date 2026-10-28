# PesoLens firmware

Runs on the Seeed XIAO ESP32-S3 Sense inside the glasses. It captures a camera frame,
runs a YOLOv8n int8 model with TensorFlow Lite Micro, sums the detected peso bills,
and speaks the total in English over an I2S amplifier and bone-conduction speaker.

Latest state and history: `../CURRENT_CHECKPOINT.md` (authoritative), `FIRMWARE_CHANGELOG.md`,
`PROGRESS.md`.

## Current build (2026-10-13)

| Item | Value |
|---|---|
| Deployed env | `seeed_xiao_esp32s3_tflm_button_capture` (D6 manual capture, stays awake) |
| Model | `model_exports/kaggle-yolov8n-imgsz256-5class-20260928/best_full_integer_quant.tflite`, 3,229,488 bytes, SHA-256 `efacda43…852f80` |
| Tensor contract | input int8 `[1,256,256,3]`, scale 1/255, zero −128; output int8 `[1,9,1344]` |
| Classes (`include/config.h`) | `20, 50, 100, 500, 1000` (no ₱200) |
| Detection | conf ≥ 0.25, class-agnostic NMS IoU 0.45, multi-bill on (`SINGLE_BILL_MODE 0`) |
| Size | flash 3,878,481 / 5,308,416 B (73.1%), RAM 6.9%, tensor arena used ≈ 866 KB of 4 MB PSRAM |
| Verified | build, upload, boot (OV3660, PSRAM, TFLite init, audio self-test beep) on 2026-10-04 |
| **Not verified** | a full D6 scan with the 256px model: inference time and spoken result on real bills |

### Scan flow (`src/main.cpp`)

Switch the latching power switch ON (it is wired in the battery line, not to D6; the
`button_capture` env builds with `PESOS_SCAN_ON_BOOT`) → boot beep → ready LED off → 3 s aim delay → capture QVGA 320×240 → center-crop 240×240 →
resize to 256×256 → inference (≈ 50 s, silent) → LED blinks 3× → speak total **or** error
tone if no bill ≥ 0.25 confidence → ready beep → LED on.

Every boot also plays a **beep** as the audio self-test. If a button press resets the board
(wiring or brownout), the user hears that beep right after pressing.

## Build and flash

The user-profile `pio.exe` is blocked by Windows Application Control on the dev PC, so
call PlatformIO through its bundled Python:

```powershell
cd C:\Users\james\Projects\PesoLens-project\firmware
$env:PLATFORMIO_CORE_DIR = "C:\.platformio"
$pio = 'C:\.platformio\penv\Scripts\python.exe'

& $pio tools\convert_model.py                                   # .tflite -> src/model_data.cpp (not in git)
& $pio -m platformio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2
& $pio -m platformio run -e seeed_xiao_esp32s3_tflm_button_capture --target upload   --upload-port COM3
& $pio -m platformio run -e seeed_xiao_esp32s3_tflm_button_capture --target uploadfs --upload-port COM3   # audio clips
& $pio -m platformio device monitor --port COM3 -b 115200
```

If upload can't reset the board: hold **BOOT**, tap **RESET**, release **BOOT**, then upload.

Host unit tests (speech composition): `pio test -e native`.

### Build environments

| Env | Purpose |
|---|---|
| `seeed_xiao_esp32s3_tflm_button_capture` | **Current.** D6 press → one scan → speak. |
| `seeed_xiao_esp32s3_tflm_button_capture_dump` | Same, plus a JPEG dump of each capture over serial. |
| `seeed_xiao_esp32s3_tflm` | Auto-scan loop every 5 s, no button. |
| `seeed_xiao_esp32s3_tflm_auto_capture_dump_8s` | No inference; dumps a JPEG every 8 s for dataset collection. |
| `seeed_xiao_esp32s3_tflm_auto_run_d6_led` | Standalone auto-scan with D6 as LED. **Do not ship:** its `sdkconfig` still puts the console on UART0 (GPIO43/44), which collides with D6 and I2S LRC. Delete `sdkconfig.<env>` and rebuild first. |
| `seeed_xiao_esp32s3` | Stub-inference recovery build (Arduino only). |
| `audio_probe`, `serial_probe` | Minimal hardware smoke tests. |

PlatformIO generates `sdkconfig.<env>` once and reuses it; editing `sdkconfig.defaults`
has no effect on an env until you delete its generated file.

## Pins (`include/config.h`)

| Function | XIAO pin | GPIO |
|---|---|---|
| Latching capture switch (OFF=open/HIGH, ON=to GND/LOW) | D6 | 43 |
| Ready LED | D5 | 6 |
| I2S DIN / BCLK / LRC → MAX98357A | D9 / D8 / D7 | 8 / 7 / 44 |
| Future deep-sleep wake button | D4 | 5 |

To scan another bill, switch OFF and ON again. On USB the board stays powered
whatever the switch position, so the switch only acts on battery.

If a separate switch or button is ever wired from D6 to GND, every push starts one scan, whichever way the switch
latches (OFF→ON or ON→OFF). The level at boot is only a baseline, so powering
on with the switch in either position does not scan. Pushes made while a scan
is running are ignored. Confirm the switch contacts provide open circuit in OFF
and connect D6 to GND in ON before powering the board.

## Debugging without the board

Run saved device JPEGs through the deployed TFLite on the PC:

```powershell
python tools\diagnose_tflite_capture.py logs\detections\<capture>.jpg --compare-letterbox
```

Capture frames from the device with `tools\monitor_detected_frames.py` while a `*_dump` env is flashed.

## Layout

```
firmware/
├── platformio.ini          build environments
├── partitions.csv          app0 @0x10000 (~5.06 MB), SPIFFS @0x520000 (~2.9 MB)
├── sdkconfig.defaults      PSRAM OPI, 240 MHz, console on USB-Serial-JTAG, task WDT 60 s
├── data/                   English WAV clips (16 kHz mono 16-bit) uploaded to SPIFFS
├── include/config.h        pins, thresholds, model size, class table
├── src/
│   ├── main.cpp            boot, button/scan loop, result → audio
│   ├── camera.cpp          OV3660 init, capture, crop/resize/quantize
│   ├── inference.cpp       TFLite Micro runner, YOLOv8 decode, NMS
│   ├── counting.cpp        detections → total pesos
│   ├── speech.cpp          number → English clip sequence
│   ├── audio.cpp           I2S + WAV streaming
│   └── power.cpp           deep sleep / EXT0 wake (for the future battery build)
├── tools/                  model conversion, clip generation, serial capture, diagnostics
├── logs/                   serial logs and captured frames from hardware sessions
└── backups/                pre-change copies of config/source (generated model arrays excluded)
```

## Known issues

- **Client report 2026-10-13:** the error tone plays on button press and no amount is spoken.
  Cause not yet confirmed. See `../CURRENT_CHECKPOINT.md`.
- The camera image is rotated 90° relative to the bill. The model still detects bills in
  test frames, but orientation was not part of validation.
- Speech clips are 14–20 dB quieter (RMS) than the beep and error tones, so words are harder
  to hear through bone conduction. Normalize or compress the clips; don't just add gain.
- Multi-bill mode adds every surviving box to the total, so a spatially separate false
  positive inflates the amount.
- ₱200 is not a model class.

## License

Project-internal student initiative. Ask before redistributing.
