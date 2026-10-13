# PesoLens Hardware Handover

Target board: Seeed Studio XIAO ESP32-S3 Sense

This file is for the hardware maker. It lists what needs to be connected to the XIAO board and what pins must be left alone.

Important: this design does not use the SYN6288 voice module anymore. Audio now uses a MAX98357A I2S amplifier and a bone-conduction speaker.

## Latest Update - 2026-05-31: D5 ready LED added

Firmware code now supports a ready indicator LED on XIAO `D5 / GPIO6`.

Current behavior:

```text
LED ON  = ESP32-S3 is initialized and waiting for the D6 capture button
LED OFF = D6 was pressed and ESP32-S3 is busy with aim delay, capture, or inference
3 blinks = bill read has finished and result/error audio is about to play
```

The LED turns ON again after the firmware has played the detected amount or
error tone and then the ready beep.

## Latest Update - 2026-05-30: D6 manual-capture firmware uploaded

The active firmware on the ESP32-S3 is now the D6 manual-capture build:

```text
PlatformIO env: seeed_xiao_esp32s3_tflm_button_capture
Latest verified port: COM3
Capture button: XIAO D6 / GPIO43 to GND, active-low
```

The firmware project contains the embedded Kaggle YOLOv8n model:

```text
model_exports\kaggle-yolov8n-imgsz224-merged-6class\best_full_integer_quant.tflite
```

Current model settings:

```text
MODEL_INPUT_SIZE = 224
MODEL_NUM_CLASSES = 6
Class order = 20, 50, 100, 200, 500, 1000
```

The latest upload and serial boot check succeeded. The board now waits for the D6 button:

```text
[mode] D6/GPIO43 manual capture enabled
[manual] D6/GPIO43 button enabled - press to start 3000 ms aim delay
[cam] init ok (RGB565 QVGA 320x240)
[infer] ready - input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980
[audio] init ok (16 kHz, 16-bit, mono)
[scan] manual mode - waiting for D6/GPIO43 press
```

Current user flow: D5 ready LED is ON while waiting, press D6, D5 turns OFF, wait 3 seconds, capture one frame, run inference, blink D5 three times with 1-second timing, speak the detected amount or error tone, play a ready beep, then D5 turns ON again. The model still takes about 32-35 seconds per inference on ESP32-S3. A later model replacement plan is saved in `FUTURE_CLASSIFIER_MODEL_PLAN.md`.

## Latest Update - 2026-05-26: Replacement ESP32 flashes and boots

The replacement Seeed XIAO ESP32-S3 Sense was connected on 2026-05-26 and detected as `COM4` with USB VID:PID `303A:1001`, MAC `e0:72:a1:fa:38:3c`.

Current result:

- Firmware build passes.
- Firmware upload to `COM4` succeeds and verifies.
- SPIFFS/audio upload to `COM4` succeeds and verifies.
- A minimal serial probe printed `[serial_probe] alive ...`, proving the replacement board boots user firmware.
- The actual PesoLens firmware now prints `[debug] alive ...` heartbeats, proving the app boots and runs.

Software fix made during this replacement-board session:

- `partitions.csv` was corrected so `app0` starts at `0x10000`, matching PlatformIO's ESP32 Arduino upload offset.
- SPIFFS remains at `0x520000`, so the audio filesystem upload still uses the intended large audio partition.

Important: the old 2026-05-23 GPIO0 / BOOT diagnosis below applies to the previous faulty board. The replacement board does not show that stuck-BOOT failure.

The current firmware build has `PESOS_DEBUG_NO_SLEEP` enabled so the board stays awake and prints serial heartbeats during smoke testing. For the final battery/deep-sleep build, comment out `-DPESOS_DEBUG_NO_SLEEP` in `platformio.ini` after hardware testing is complete.

## Latest Update — 2026-05-23: Boot problem confirmed to be HARDWARE (GPIO0 / BOOT held low)

The test board was connected and fully debugged on 2026-05-23. Firmware build and USB upload both work, but the board does **not** run any firmware: on every power-up it boots straight into the chip's USB download / bootloader mode. This was traced to **GPIO0 / BOOT being held electrically LOW at boot** — a hardware fault on this specific unit, **not** a firmware, PC, or PlatformIO problem.

- **How it was proven:** flashed a bare LED-blink program (no serial, no sleep, minimal config) and did a clean power cycle. The LED never blinked, and the flashing tool could still load its helper "stub" — which only works in download mode. After a true power-on reset (which re-reads the boot straps), the only thing that causes this is GPIO0 read LOW.
- **Action for the hardware maker:** inspect/repair the BOOT / GPIO0 circuit (see "If The Board Is Stuck In Bootloader" below). Multimeter continuity GPIO0 ↔ GND with BOOT **not** pressed should be OPEN; if it beeps, GPIO0 is shorted low = root cause. Likely culprits: stuck BOOT button, solder bridge / flux / debris on the BOOT pads, a stray wire touching GPIO0, or a damaged pad.
- **Full write-up to forward:** `FINAL_DIAGNOSIS.txt` in this firmware folder.
- **Once a fixed/replacement board is in hand:** the existing firmware flashes and runs normally — no software change is needed for this boot issue.

## Which Folder To Use

There are multiple PesoLens-related folders on this computer. They are related, but they are not the same project folder.

Use this folder for ESP32-S3 firmware builds, uploads, serial monitor logs, and hardware handoff work:

```powershell
C:\Users\james\Projects\PesoLens-project\firmware
```

This is the folder that contains `platformio.ini`, `src`, `include`, `data`, and the `.pio` build output. PlatformIO upload commands must be run here.

The other PesoLens folders are Python/model packages, not firmware upload folders:

| Folder | What It Is For |
|---|---|
| `C:\Users\james\Projects\PesoLens-project\PesoLens-training` | Model training, Kaggle outputs, TFLite exports, prediction outputs, and training notes. |
| `C:\Users\james\Projects\PesoLens-project\firmware` | ESP32-S3 firmware, audio clips, embedded model bytes, PlatformIO config, and serial logs. |

Do not run ESP32-S3 upload commands from those model folders. For firmware flashing, always switch to:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
```

## Simple Wiring Checklist

Wire these parts:

| Part | Connect This Pin | To This XIAO Pin | Notes |
|---|---|---|---|
| MAX98357A amplifier | DIN or DATA | D9 / GPIO8 | Audio data signal |
| MAX98357A amplifier | BCLK, BCK, or SCK | D8 / GPIO7 | Audio clock signal |
| MAX98357A amplifier | LRC, WS, or LRCLK | D7 / GPIO44 | Audio left/right clock |
| MAX98357A amplifier | VIN or VCC | 3V3 or module-rated power | Check the exact amp board. Many accept 3V to 5V. |
| MAX98357A amplifier | GND | GND | Must share ground with the XIAO |
| MAX98357A amplifier | SD, SD_MODE, or EN | leave unconnected, or 3V3 through 100k | **Silent-amp culprit #1.** This pin is both shutdown and channel select. Held below ~0.16 V the amp is fully shut down and outputs nothing no matter what I2S sends. Breakout vendors bias it differently, so never assume "unconnected" means enabled -- measure it. See the voltage table below. |
| Bone-conduction speaker | speaker wires | MAX98357A speaker output | Do not connect speaker wires directly to ESP32 pins |
| Current capture push button | one side | D6 / GPIO43 | Active-low manual capture button in the latest uploaded firmware |
| Current capture push button | other side | GND | Pressing it starts the 3-second aim delay and capture |
| Ready indicator LED | signal side | D5 / GPIO6 | Active-high by default: ON means ready for D6 capture |
| Ready indicator LED | other side | GND through resistor | If wired to 3V3 instead, invert `READY_LED_ON_LEVEL` / `READY_LED_OFF_LEVEL` |
| Previous/final wake button | one side | D4 / GPIO5 | Older deep-sleep wake/trigger plan; not the active trigger in the current USB manual-capture build |
| Previous/final wake button | other side | GND | Keep for later battery/deep-sleep work if needed |
| Battery / power system | BAT+ and BAT- | board power or charger circuit | Follow the chosen battery/charging module design |

## MAX98357A SD / SD_MODE Pin

If the amplifier is silent while the serial log still prints `[audio] playing ... (Nms)`,
measure the voltage on `SD` with the board powered. The log line only proves bytes
were pushed into the I2S DMA buffer -- it has no feedback path from the amp or the
speaker, so it prints identically when nothing is connected at all.

| Voltage on SD | Amplifier behavior |
|---|---|
| below ~0.16 V | Shut down. Completely silent. |
| ~0.16 V to ~0.77 V | Plays (left + right) / 2 |
| ~0.77 V to ~1.4 V | Plays right channel only |
| above ~1.4 V | Plays left channel only |

`src/audio.cpp` duplicates every mono sample onto both channels, so any of the three
enabled modes produces sound. Only the shutdown case is silent.

## What Not To Connect

Do not connect the SYN6288 module. It was removed from this design.

Do not connect the speaker directly to the XIAO ESP32-S3 pins. The speaker must connect to the MAX98357A amplifier output.

Do not reuse the camera pins listed below. They are already used internally by the XIAO ESP32-S3 Sense camera.

## Camera Pins To Leave Alone

These pins are used by the camera connection. Treat them as reserved.

| Camera Signal | ESP32-S3 GPIO |
|---|---:|
| XCLK | GPIO10 |
| PCLK | GPIO13 |
| VSYNC | GPIO38 |
| HREF | GPIO47 |
| SIOD / SDA | GPIO40 |
| SIOC / SCL | GPIO39 |
| Y2 / D0 | GPIO15 |
| Y3 / D1 | GPIO17 |
| Y4 / D2 | GPIO18 |
| Y5 / D3 | GPIO16 |
| Y6 / D4 | GPIO14 |
| Y7 / D5 | GPIO12 |
| Y8 / D6 | GPIO11 |
| Y9 / D7 | GPIO48 |
| PWDN | not connected |
| RESET | not connected |
| Camera LED | not connected |

Important: the camera table's `Y7 / D5` and `Y8 / D6` labels are camera
data-signal names on `GPIO12` and `GPIO11`. They are not the same as the XIAO
board's external `D5 / GPIO6` ready LED pin or external `D6 / GPIO43` capture
button pin.

## What The Button Should Do

For the current uploaded debug/demo firmware, the capture button is connected between XIAO D6 / GPIO43 and GND.

When the user presses D6, the board turns the D5 ready LED off, waits 3 seconds, captures one frame, runs the peso model, blinks the D5 LED 3 times before audio, speaks the result or error tone, then plays a beep and turns the D5 ready LED on when it is ready for the next capture.

The older D4 / GPIO5 button was the intended wake-from-deep-sleep trigger. Treat D4 as a future battery/deep-sleep pin unless the firmware is changed back to that mode.

## Firmware Upload

This section is only needed if the hardware maker will also upload firmware to the board.

Update 2026-05-26: the replacement board WAS connected (COM4), firmware upload/verify succeeded, SPIFFS/audio upload/verify succeeded, and serial logs confirm the PesoLens app is running. The old 2026-05-23 COM3 GPIO0 / BOOT diagnosis applies to the previous faulty board only.

Current firmware status as of 2026-05-30:

| Check | Result |
|---|---|
| Previous ESP32-S3 firmware build | Passed with `pio run -e seeed_xiao_esp32s3` |
| Previous audio filesystem image | Passed with `pio run -e seeed_xiao_esp32s3 --target buildfs` and `--target uploadfs --upload-port COM4` |
| Native speech tests | Passed, 15/15 tests |
| Connected USB board | Replacement board latest detected on `COM3` |
| Runtime boot logs | Passed: serial capture shows `[debug] alive ...` heartbeats |
| Previous real TFLite inference | Was enabled and initialized with the older 160 model |
| New 224/6-class firmware | Built, uploaded, and verified to boot with `[infer] ready` |
| D6 manual capture firmware | Uploaded as `seeed_xiao_esp32s3_tflm_button_capture`; serial logs show it waiting for D6/GPIO43 |
| Real bill test with D6 manual flow | Still needs controlled testing |

## Current TFLite Micro Note

The new Kaggle 224/6-class model is inside the firmware folder, embedded into `src/model_data.cpp`, built, uploaded, and verified to initialize on-device.

- Use `pio run -e seeed_xiao_esp32s3` for the known-good USB/stub recovery build.
- Use `pio run -e seeed_xiao_esp32s3_tflm` for the real TFLite Micro build.
- Use `pio run -e seeed_xiao_esp32s3_tflm_button_capture` for the current D6 manual-capture build.
- Use `pio run -e seeed_xiao_esp32s3_tflm_button_capture_dump` only if a serial JPEG frame dump is needed for visual capture debugging.
- Do not treat live bill recognition as stable until controlled D6 bill logs are collected.

### Option A: Upload With VS Code And PlatformIO

1. Install Visual Studio Code.
2. Install the PlatformIO extension.
3. Open this folder:

```powershell
C:\Users\james\Projects\PesoLens-project\firmware
```

4. Connect the XIAO ESP32-S3 Sense with a USB-C data cable.
5. In PlatformIO, choose the `seeed_xiao_esp32s3_tflm_button_capture` environment for the current D6 manual-capture firmware.
6. Run Upload.
7. Also upload the filesystem image so the audio files are installed.

### Option B: Upload Without VS Code

This still uses PlatformIO, but from a terminal instead of VS Code.

Open a terminal in the firmware folder:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
```

Build the firmware:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture -j 2
```

Upload the firmware:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target upload --upload-port COM3
```

Upload the audio files:

```powershell
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target uploadfs --upload-port COM3
```

Open the serial monitor:

```powershell
pio device monitor --port COM3 -b 115200
```

### If The Board Is Stuck In Bootloader

If the board keeps entering bootloader/download mode or the upload/reset cycle is unreliable, check the BOOT strap before changing firmware.

- On the XIAO ESP32-S3, the BOOT pin is GPIO0.
- The XIAO silkscreen pin `D0` is GPIO1, so do not confuse board pin D0 with BOOT/GPIO0.
- This PesoLens firmware does not intentionally use GPIO0 or D0 for any external part.
- The current manual capture button is D6 / GPIO43 to GND.
- The older intended deep-sleep wake button is D4 / GPIO5 to GND.
- BOOT/GPIO0 should not be held low during normal reset. It should only be pulled low when intentionally entering download/upload mode.

Hardware checks:

| Check | Expected Result |
|---|---|
| BOOT button | Not stuck pressed |
| BOOT/GPIO0 wiring | No external module or wire pulling it to GND |
| Reset/EN button | Can reset the board after upload |
| D6/GPIO43 button | Current capture button only, one side to GPIO43 and the other side to GND |
| D5/GPIO6 LED | Current ready indicator only, active-high unless firmware levels are inverted |
| D4/GPIO5 button | Older/future wake button only, one side to GPIO5 and the other side to GND if still installed |
| D0/GPIO1 | Not used by this firmware |
| Camera pins | Leave the reserved camera pins alone |

Recommended upload sequence if auto-upload fails:

1. Disconnect external modules from the board, especially anything touching BOOT/GPIO0, D0/GPIO1, EN, or GND.
2. Plug in the USB-C data cable.
3. Run `pio device list` and identify the COM port.
4. Hold BOOT, tap RESET, then release BOOT.
5. Run `pio run -e seeed_xiao_esp32s3 --target upload`.
6. After upload finishes, tap RESET once so the board leaves bootloader mode and starts the firmware.
7. Run `pio run -e seeed_xiao_esp32s3 --target uploadfs` so the audio files are installed.
8. Run `pio device monitor -b 115200` and press the D6/GPIO43 capture button.

If the board only uploads while external wiring is disconnected, the wiring is influencing a boot strap or reset line and should be fixed before the hardware smoke test.

### Advanced Option: esptool.py

There is a lower-level upload method using `esptool.py`, but do not use it unless the programmer provides a tested binary bundle and exact command.

Manual flashing needs the correct bootloader, partition table, firmware image, filesystem image, and flash offsets. Guessing these values can make the board fail to boot.

## Hardware Smoke Test

After wiring and firmware upload:

1. Power the board.
2. Open the serial monitor if available.
3. Confirm the D5 / GPIO6 ready LED turns on after initialization.
4. Press the button on D6 / GPIO43.
5. The D5 ready LED should turn off while the board waits 3 seconds, captures one frame, and runs inference.
6. After inference, the D5 ready LED should blink 3 times with 1-second timing before the result/error audio.
7. The board should play the result/error audio, play a ready beep, then turn the D5 ready LED on again.
8. Audio should come from the speaker through the MAX98357A amplifier.

If there is no audio, check:

| Check | What To Confirm |
|---|---|
| Audio files | The filesystem/audio upload was done with `uploadfs` |
| Amplifier wiring | DIN is on D9/GPIO8, BCLK is on D8/GPIO7, LRC is on D7/GPIO44 |
| Power | MAX98357A has power and GND |
| Ground | MAX98357A GND and XIAO GND are connected together |
| Speaker | Speaker is connected to MAX98357A output, not directly to the XIAO |

## Firmware Reference Files

These are for the programmer, not required for normal wiring:

| File | Purpose |
|---|---|
| `include/config.h` | Main external pin definitions |
| `include/camera_pins.h` | Reserved camera pin definitions |
| `src/audio.cpp` | MAX98357A / I2S audio code |
| `src/power.cpp` | Button wake and sleep behavior |
