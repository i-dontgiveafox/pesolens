# ESP32-S3 Hardware Test Checklist

Date: 2026-05-30

Project folder:

```text
C:\Users\james\Projects\PesoLens-project\firmware
```

## Current Status

The ESP32-S3 has the current D6 manual-capture firmware uploaded and is ready for controlled bill tests.

Done:

- Previous real TFLite firmware built and uploaded to `COM4`.
- Previous firmware fit the 8 MB flash layout.
- Board boots and prints `[debug] alive ...`, so upload and boot are confirmed.
- Audio filesystem was previously uploaded.
- New merged 6-class `imgsz=224` full-integer TFLite model has been embedded into firmware.
- Current firmware environment `seeed_xiao_esp32s3_tflm_button_capture` was uploaded on `COM3`.
- Latest serial boot check confirmed camera init, `[infer] ready`, audio init, and waiting for D6/GPIO43.

Not fully proven yet:

- Real bill recognition has not been verified with the new model on-device.
- The current TFLM build uses D6 manual capture for USB/debug testing instead of normal deep sleep behavior.
- The current YOLO model still takes about 32-35 seconds per inference after the 3-second aim delay.

## Safe Hardware Test Order

1. Disconnect USB power before wiring anything.
2. Wire the MAX98357A amplifier first.
3. Wire the push button next.
4. Wire the D5 ready LED next.
5. Leave all camera pins alone.
6. Avoid `GPIO0 / BOOT`, `EN`, and any reset/boot strap pins.
7. Reconnect USB and watch serial logs.

## MAX98357A Wiring

| MAX98357A pin | XIAO ESP32-S3 Sense pin |
|---|---|
| `DIN` | `D9 / GPIO8` |
| `BCLK` | `D8 / GPIO7` |
| `LRC` | `D7 / GPIO44` |
| `GND` | `GND` |
| `VIN` | Amp-supported power input |

## Button Wiring

| Button side | XIAO ESP32-S3 Sense pin |
|---|---|
| One side | `D6 / GPIO43` |
| Other side | `GND` |

The button is active-low. Pressing it connects `GPIO43` to `GND`.

The older D4/GPIO5 button was the deep-sleep wake plan. It is not the active capture trigger in the current uploaded debug firmware.

## Ready LED Wiring

| LED side | XIAO ESP32-S3 Sense pin |
|---|---|
| Signal/anode side | `D5 / GPIO6` |
| Other side | `GND` through a resistor |

The firmware assumes active-high wiring. The LED is ON when the ESP32-S3 is
ready for a D6 capture, OFF after D6 is pressed during the 3-second aim delay,
capture, and inference, then blinks 3 times with 1-second timing before audio
playback.

## Do Not Connect External Parts To

- `GPIO0 / BOOT`
- `EN`
- Camera pins
- USB data pins
- Any pin already reserved by the camera module

## Serial Monitor

Use this from PowerShell:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR = "C:\.platformio"
pio device monitor --port COM3 -b 115200
```

Expected current manual-capture boot sign:

```text
[mode] D6/GPIO43 manual capture enabled
[manual] D6/GPIO43 button enabled - press to start 3000 ms aim delay
[cam] init ok (RGB565 QVGA 320x240)
[infer] ready - input [1,224,224,3] int8, output [1,10,1029] (C,A), arena=4194304 bytes PSRAM, used=782980
[audio] init ok (16 kHz, 16-bit, mono)
[scan] manual mode - waiting for D6/GPIO43 press
[ready] D5/GPIO6 LED on - ready for capture
```

Expected D6 press sign:

```text
[ready] D5/GPIO6 LED off - capture busy
[manual] D6 capture button pressed - capture in 3000 ms
[manual] capture delay done
[scan] capture start
[infer] invoke=... ms
[result] ... pesos from ... bills
[ready] D5/GPIO6 LED blink 3 times before audio
[speak] ...
[ready] beep
[ready] D5/GPIO6 LED on - ready for next capture
```

## If Audio Does Not Play

Re-upload the SPIFFS audio filesystem:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\firmware"
$env:PLATFORMIO_CORE_DIR = "C:\.platformio"
pio run -e seeed_xiao_esp32s3_tflm_button_capture --target uploadfs --upload-port COM3
```

## Practical Testing Notes

- Start with USB-only testing before battery testing.
- If the board stops appearing on `COM3`, disconnect external wiring and test the board alone. Use `pio device list` because the COM port can change.
- If upload works only when wiring is disconnected, one of the external connections is interfering with a boot or reset pin.
- Treat real peso detection as the next validation step, not already proven.
- If `AllocateTensors failed` appears, increase `TENSOR_ARENA_SIZE` in `include/config.h`.
- If inference is too slow, use `FUTURE_CLASSIFIER_MODEL_PLAN.md` for the later tiny-classifier path.
