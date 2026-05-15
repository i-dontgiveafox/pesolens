# PesoLens Firmware Hardware Pin Handover

Target board: Seeed Studio XIAO ESP32-S3 Sense

This firmware no longer uses the SYN6288 TTS module. Audio is generated from stored WAV clips and sent through I2S to a MAX98357A amplifier.

## External Wiring Required

| Component | Component Pin | Connect To On XIAO | ESP32-S3 GPIO | Firmware Symbol | Notes |
|---|---|---|---:|---|---|
| MAX98357A I2S amp | DIN / DATA | D1 | GPIO2 | I2S_DIN | Audio data from ESP32 to amp |
| MAX98357A I2S amp | BCLK / BCK / SCK | D2 | GPIO3 | I2S_BCLK | I2S bit clock |
| MAX98357A I2S amp | LRC / WS / LRCLK | D3 | GPIO4 | I2S_LRC | I2S word select / left-right clock |
| MAX98357A I2S amp | VIN / VCC | 3V3 or module-rated supply | n/a | n/a | Check the exact amp board. Many MAX98357A boards accept 3V to 5V. |
| MAX98357A I2S amp | GND | GND | n/a | n/a | Must share ground with XIAO |
| Bone-conduction speaker | + / - | MAX98357A speaker output | n/a | n/a | Speaker connects to the amp output, not directly to ESP32 pins |
| Push button | one side | D4 | GPIO5 | BUTTON_PIN | Active-low wake/trigger button |
| Push button | other side | GND | n/a | n/a | Firmware uses internal pull-up and EXT0 wake |
| Battery | BAT+ / BAT- | board power input / charger path | n/a | n/a | Follow the selected power/charging module wiring |

## Do Not Wire SYN6288

The SYN6288 has been removed from this design. Do not connect it to the XIAO for the current firmware.

Reason: SYN6288 is a UART/analog-output TTS module, while this firmware uses I2S audio through the MAX98357A amplifier and stored English WAV clips.

## Pins Reserved By The Camera

These pins are already used by the XIAO ESP32-S3 Sense camera connection. Do not reuse them for the amp, button, battery sensing, or any other external part.

| Camera Signal | ESP32-S3 GPIO | Notes |
|---|---:|---|
| XCLK | GPIO10 | Camera clock |
| PCLK | GPIO13 | Pixel clock |
| VSYNC | GPIO38 | Camera vertical sync |
| HREF | GPIO47 | Camera horizontal reference |
| SIOD / SDA | GPIO40 | Camera SCCB data |
| SIOC / SCL | GPIO39 | Camera SCCB clock |
| Y2 / D0 | GPIO15 | Camera data |
| Y3 / D1 | GPIO17 | Camera data |
| Y4 / D2 | GPIO18 | Camera data |
| Y5 / D3 | GPIO16 | Camera data |
| Y6 / D4 | GPIO14 | Camera data |
| Y7 / D5 | GPIO12 | Camera data |
| Y8 / D6 | GPIO11 | Camera data |
| Y9 / D7 | GPIO48 | Camera data |
| PWDN | not connected | Firmware value is -1 |
| RESET | not connected | Firmware value is -1 |
| Camera LED | not connected | Firmware value is -1 |

## Firmware References

- External pin definitions: `include/config.h`
- Camera pin definitions: `include/camera_pins.h`
- Audio driver path: `src/audio.cpp`
- Power/button wake path: `src/power.cpp`

## Firmware Setup And Upload

Use this section when flashing the Seeed Studio XIAO ESP32-S3 Sense.

### 1. Install Required Software

On the development computer, install:

- Visual Studio Code
- PlatformIO extension for VS Code
- Git, if the project is being cloned from a repository
- A USB-C cable that supports data, not charge-only

PlatformIO can also be run from a terminal if it is already installed.

### 2. Open The Firmware Folder

Open this folder in VS Code or a terminal:

```powershell
C:\Users\Lester Valdez\Downloads\pesos-firmware-v1\firmware
```

The file `platformio.ini` should be visible in the folder root.

### 3. Connect The Board

1. Connect the XIAO ESP32-S3 Sense to the computer over USB-C.
2. Select the `seeed_xiao_esp32s3` PlatformIO environment.
3. If upload fails because the board is not detected, press reset once and retry. If it still fails, use the XIAO bootloader procedure for the board revision being used.

### 4. Build The Firmware

Run:

```powershell
pio run -e seeed_xiao_esp32s3
```

Expected result: PlatformIO should finish with `SUCCESS`.

Current verified status: this build passes with stub inference mode. The real TensorFlow Lite Micro inference path is still gated off by `ENABLE_TFLITE_MICRO=0` in the firmware.

### 5. Upload The Firmware

Run:

```powershell
pio run -e seeed_xiao_esp32s3 --target upload
```

This flashes the compiled firmware to the XIAO ESP32-S3 Sense.

### 6. Build And Upload The Audio Files

The firmware plays stored WAV files from SPIFFS, so the filesystem image also needs to be uploaded.

Build the SPIFFS image:

```powershell
pio run -e seeed_xiao_esp32s3 --target buildfs
```

Upload the SPIFFS image:

```powershell
pio run -e seeed_xiao_esp32s3 --target uploadfs
```

### 7. Open Serial Monitor

Run:

```powershell
pio device monitor -b 115200
```

Expected serial speed: `115200`.

### 8. Basic Hardware Smoke Test

After flashing firmware and SPIFFS:

1. Power the board.
2. Confirm the serial monitor prints boot or wake messages.
3. Press the button wired to `D4 / GPIO5`.
4. The firmware should wake, attempt capture/count flow, play audio through the MAX98357A path, then return to sleep.

If there is no audio, check these first:

- SPIFFS was uploaded with `uploadfs`
- MAX98357A `DIN`, `BCLK`, and `LRC` match the wiring table above
- MAX98357A has power and common ground with the XIAO
- Speaker is connected to the MAX98357A speaker output, not directly to ESP32 pins
