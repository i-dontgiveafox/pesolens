# Deployment — build, flash, monitor

Quick reference. Assumes Python 3.12 + PlatformIO Core (`pip install platformio`) + ffmpeg are already installed. If not, see `SETUP_GUIDE.md` section 4.

All commands run from `C:\Users\David\Desktop\Projects\pesos\firmware\`.

## One-time prep (or whenever model/audio changes)

Convert the trained model into a C header the firmware can compile in:

```
python tools/convert_model.py
```

Produces `include/model_data.h`.

Generate every English voice clip:

```
python tools/generate_english_clips.py
```

Writes ~35 WAV files (16 kHz mono 16-bit) into `data/`. Idempotent — re-running skips existing clips.

## Build

```
pio run
```

Look for `[SUCCESS]` and a memory-usage report. Output binary lives at `.pio/build/seeed_xiao_esp32s3/firmware.bin`.

## Flash the firmware

XIAO plugged in over USB-C (data-capable cable):

```
pio run --target upload
```

If the port isn't auto-detected, hold the XIAO's BOOT button while plugging it in, then re-run.

## Flash the audio

```
pio run --target uploadfs
```

This packs `data/` into a SPIFFS image and writes it to the dedicated audio partition.

## Monitor

```
pio device monitor
```

Default baud is 115200. `Ctrl+]` to exit.

## Full clean re-flash (rare)

```
pio run --target erase
pio run --target upload
pio run --target uploadfs
```

`erase` wipes flash including NVS — use it only if the board is in a confused state.

## Common one-liners

```
pio run -e seeed_xiao_esp32s3                # build the default env explicitly
pio run --target upload --upload-port COM5   # force a specific port (Windows)
pio device list                              # what serial ports does PlatformIO see?
```
