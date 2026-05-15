# pesos — Setup Guide

End-to-end walkthrough from "I just got the parts" to "the glasses tell me how much money is in my hand." Written for someone who is **not** a developer. No jargon, no skipped steps.

---

## 1. What this is

`pesos` is a pair of assistive eyeglasses for visually-impaired users. Point your face at a stack of Philippine peso bills, press a button on the side, and a small speaker that touches the bone behind your ear quietly says the total in **English** — "seven hundred thirty pesos." That's it. No phone, no internet, no cloud. Everything happens on a coin-sized computer mounted on the temple of the glasses.

**Who it's for:** people in the Philippines who can't see well enough to count cash by hand. The bills look almost identical by touch, and that makes daily life harder than it needs to be.

**How it works at a high level:**

```
    [you press the button]
            |
            v
    [the tiny computer wakes up]
            |
            v
    [the camera takes one picture]
            |
            v
    [the AI model spots each bill in the picture
     and labels it 20 / 50 / 100 / 200 / 500 / 1000]
            |
            v
    [the code adds them up]
            |
            v
    [it picks the right pre-recorded English words
     and plays them through the bone-conduction speaker]
            |
            v
    [the computer goes back to sleep, sipping almost no battery]
```

The whole cycle takes about a second. Then the device is asleep again, drawing so little power that the small battery lasts well over a day of normal use.

---

## 2. Hardware shopping list

These are the parts you actually need. **One important correction up front:** the original parts list mentioned a "SYN6288" voice chip. We dropped it — it can't speak English the way we need, and the audio it produces doesn't match the rest of the wiring. We replaced it with pre-recorded English voice clips that we generate on a laptop in 30 seconds. **A small push button was also added** so the user can trigger a reading on demand. Everything else stays the same.

| # | Part | Required? | Where to buy in PH | International |
|---|---|---|---|---|
| 1 | Seeed XIAO ESP32-S3 **Sense** (this is the version *with* the built-in camera — make sure it says "Sense") | Required | Lazada/Shopee — search "XIAO ESP32-S3 Sense Seeed" | Seeedstudio.com, Amazon, Adafruit |
| 2 | MAX98357A I2S audio amplifier breakout board | Required | Lazada/Shopee — search "MAX98357A I2S amplifier" | Adafruit #3006, Amazon |
| 3 | Bone-conduction speaker, 8 ohm, small (around 2-3 cm) | Required | Lazada/Shopee — search "bone conduction transducer 8 ohm" | Adafruit #1674, Amazon, AliExpress |
| 4 | Tactile push button (any momentary switch with two leads) | Required | Lazada/Shopee — pack of 10 for under ₱100 | Amazon, any electronics shop |
| 5 | 3.7 V LiPo battery, 600 mAh, with JST-PH 1.25 mm connector | Required | Lazada/Shopee — "600mAh lipo JST 1.25" | Adafruit #1578, Amazon |
| 6 | Eyeglass frame (any frame you can mount the parts to — old reading glasses work) | Required | Any optical shop | Any optical shop |
| 7 | Thin hookup wire (24-30 AWG, silicone-jacketed is easiest to bend along the temple) | Required | Lazada/Shopee — "silicone wire 28 AWG" | Amazon, Adafruit |
| 8 | Heatshrink tubing or electrical tape | Required | Any hardware store | Any hardware store |
| 9 | 220 µF electrolytic capacitor (smooths out power spikes when the camera fires) | Nice-to-have | Any electronics shop | Any electronics shop |
| 10 | Small soldering iron + solder | Required to assemble | Lazada/Shopee | Amazon |
| 11 | Hot glue or 5-minute epoxy (to mount parts to the frame) | Required to assemble | Any hardware store | Any hardware store |
| 12 | USB-C cable (data, not just charging — most are fine but cheap ones are charge-only) | Required | Anywhere | Anywhere |

Total cost in the Philippines is roughly ₱1,800-₱2,500 depending on which seller you go with.

---

## 3. Wiring diagram

Every connection from the tiny computer (the XIAO) to the other parts. Pin numbers refer to the silkscreen labels printed on the XIAO Sense board.

```
                       Seeed XIAO ESP32-S3 Sense
                       +-------------------------+
                       |                         |
        I2S DIN    ----| D1  (GPIO 2) ---------> MAX98357A DIN
                       |                         |
        I2S BCLK   ----| D2  (GPIO 3) ---------> MAX98357A BCLK
        I2S LRC    ----| D3  (GPIO 4) ---------> MAX98357A LRC
                       |                         |
        button     ----| D4  (GPIO 5) ----+      |
                       |                  |      |
                       |             [push button]
                       |                  |      |
                       |                  +-----> GND  (button's other leg)
                       |                         |
        camera ribbon  | (built into board, no   |
                       |   wiring needed for it) |
                       |                         |
        battery +  ----| BAT+  (JST connector)   |
        battery -  ----| BAT-  (JST connector)   |
                       |                         |
        3.3V power -+--| 3V3                     |
                    |  | GND  -------+           |
                    |  |             |           |
                    |  +-------------+--- (shared ground rail)
                    |                            |
                    +-------+                    |
                            |                    |
                       +----v---------+          |
                       | MAX98357A    |          |
                       |              |          |
                       |  VIN <----- 3V3 from XIAO
                       |  GND <----- GND  (shared ground)
                       |  DIN <----- D1  (audio data, GPIO 2)
                       |  BCLK<----- D2  (audio bit clock, GPIO 3)
                       |  LRC <----- D3  (audio left/right clock, GPIO 4)
                       |  GAIN open  (default 9 dB, fine for bone speaker)
                       |  SD   open  (always on — see "future improvements")
                       |              |
                       |  +out -------+----> bone-conduction speaker (+)
                       |  -out -------+----> bone-conduction speaker (-)
                       +--------------+
```

**Important:** D1 is used **only** for I2S audio data going to the MAX98357A. The button is on D4 only. Each pin does exactly one job.

**Pin table — quick reference**

| XIAO pin | Connects to | Why |
|---|---|---|
| D1 (GPIO 2) | MAX98357A `DIN` | sends digital audio bits out |
| D2 (GPIO 3) | MAX98357A `BCLK` | audio bit clock |
| D3 (GPIO 4) | MAX98357A `LRC` | audio left/right clock |
| D4 (GPIO 5) | one side of push button (other side to GND) | wake-from-sleep trigger |
| 3V3 | MAX98357A `VIN` | 3.3 V power for the amp |
| GND | shared ground for amp and button | reference voltage |
| BAT+ / BAT- | LiPo battery JST connector | power |

**Tip:** solder the 220 µF capacitor between 3V3 and GND on the breadboard side, near the XIAO. It smooths out the brief power dip when the camera fires up. Without it, the board can occasionally reboot itself ("brownout") on a fresh battery that's a little weak.

---

## 4. Software install

You only need to do this once on your laptop. We're using a free toolset called PlatformIO — think of it as the workshop that turns the code into a tiny program the XIAO can run.

### 4a. Install Python 3.12

- **Windows:** download the installer from python.org, run it. **Tick "Add python.exe to PATH"** at the bottom of the first screen. That's the most common stumble.
- **macOS:** `brew install python@3.12` (if you have Homebrew) or download from python.org.
- **Linux:** `sudo apt install python3.12 python3-pip` on Debian/Ubuntu, or use your distribution's package manager.

Verify it works: open a fresh terminal and run `python --version`. You should see `Python 3.12.x`.

### 4b. Install PlatformIO Core

In the same terminal:

```
pip install platformio
```

Verify: `pio --version` should print something like `PlatformIO Core, version 6.x`.

### 4c. Install VS Code (recommended)

VS Code is a free code editor. Once installed, open it and:
1. Click the squares icon on the left (Extensions).
2. Search for "PlatformIO IDE" and click Install.
3. Wait for it to finish setting up — it will show a popup saying "PlatformIO is ready."

VS Code is technically optional — every step in this guide works from a plain terminal too — but the green "play" and "upload" buttons in the PlatformIO sidebar are nicer than typing commands.

### 4d. Install edge-tts (the voice generator)

This is what creates the English voice clips for us. In the same terminal:

```
pip install edge-tts pydub
```

You'll also want `ffmpeg` installed on your system for the cleanest WAV conversion. On Windows, the easiest path is `winget install Gyan.FFmpeg`. On macOS, `brew install ffmpeg`. On Linux, `sudo apt install ffmpeg`.

---

## 5. Project layout

The project lives in `C:\Users\David\Desktop\Projects\pesos\`. Here's what each folder is for, in plain terms.

- **`datasets/`** — the labelled photos of peso bills used to teach the AI model what each bill looks like. Already in place from training night; you don't usually touch this again.
- **`scripts/`** — the Python scripts that train the AI model on a desktop GPU. The training has already been run, so these are reference only.
- **`runs/`** — where the trained AI model files live after training. The one we care about is `runs/pesos-small/weights/best.pt` (and its converted `.tflite` form), which is the small version that actually fits on the glasses.
- **`firmware/`** — the code that runs **on the glasses themselves**. This is the main folder you'll work with from now on. It contains the program, the audio clips, the AI model, and the build configuration.
- **`docs/`** — the documents you're reading right now and a few shorter cheat sheets.
- **`firmware/tools/`** — small helper Python scripts. One converts the AI model into a form the glasses can load, one generates the English voice clips, and one fills in placeholder audio so the project compiles even before real audio exists.

---

## 6. Preparing the model

The "model" is the AI brain. It's a file that the glasses load into memory at startup so they can recognize bills.

### What already happened

`scripts/train_small.py` was run earlier on a desktop computer with a strong graphics card. It taught a small AI model — a YOLOv8n at 320×320 pixel input — to recognize the six Philippine bills (₱20, ₱50, ₱100, ₱200, ₱500, ₱1000). At the end of training, it automatically wrote out a compact version of the model called a TFLite int8 model. That smaller version is what the glasses can actually run.

The compact model lives at:

```
runs/pesos-small/weights/best_saved_model/best_int8.tflite
```

It's about 3 MB.

### Turning it into something the firmware can include

The firmware doesn't load the model from a separate file at startup — it bakes the model into the program itself. To do that we run a small conversion script that turns the `.tflite` file into a C header file:

```
cd firmware
python tools/convert_model.py
```

This produces `firmware/include/model_data.h`, which is automatically picked up the next time you build the firmware. **You only need to re-run this if you ever retrain the model.**

---

## 7. Preparing the audio

Instead of buying a voice chip, we generate every English word the glasses need to say once, save them as small audio files, and the firmware plays them in sequence.

### Generate every clip in one go

```
cd firmware
python tools/generate_english_clips.py
```

That script uses Microsoft's free neural voice service (`edge-tts`) to synthesize a clean, natural English voice for each word. It runs through the full list in about half a minute and saves the WAV files into `firmware/data/`. Run it again later and it skips clips that already exist, so it's safe to re-run.

What gets generated:

- digits (zero through nine)
- the teens (ten through nineteen)
- the tens (twenty, thirty, forty, fifty, sixty, seventy, eighty, ninety)
- the multipliers (hundred, thousand)
- the connector "and"
- "peso" (singular) and "pesos" (plural)
- a short confirmation beep
- a short error tone

Total: ~35 clips, roughly 600-900 KB at 16 kHz mono 16-bit.

### Want a different voice?

The default is `en-US-AriaNeural` — clear, warm, well-paced for assistive use. Pass another voice name as a command-line argument to override:

```
python tools/generate_english_clips.py --voice en-GB-SoniaNeural
```

`edge-tts --list-voices` prints every available voice.

---

## 8. Building

"Building" means compiling the code into a `.bin` file that the XIAO can run.

```
cd firmware
pio run
```

The first time you do this, PlatformIO downloads the right toolchain for the ESP32-S3 — that takes a few minutes. After that, builds take 10-30 seconds.

**What success looks like:** the last line says `[SUCCESS]` in green, and a file appears at `firmware/.pio/build/seeed_xiao_esp32s3/firmware.bin`. Memory usage will be reported above that — you want both Flash and RAM under 100 % (typically Flash sits around 60-70 %, RAM around 20-25 %).

---

## 9. Flashing

"Flashing" means copying the program onto the XIAO over USB.

1. Plug the XIAO into your laptop with a USB-C data cable.
2. From the `firmware/` folder:

```
pio run --target upload
```

This sends the program over.

3. Then send the audio clips, which live on a separate small file system on the chip:

```
pio run --target uploadfs
```

4. Watch what the device says over the serial connection (helpful for debugging):

```
pio device monitor
```

Press `Ctrl+]` to exit the monitor.

If the upload says "no port found," try a different USB cable (charge-only cables are common and silently fail) or hold down the BOOT button on the XIAO while plugging it in.

---

## 10. Using it

1. Plug the LiPo battery into the JST connector on the XIAO. The device boots, plays a single beep, then drops into deep sleep.
2. Hold a stack of bills 15-30 cm in front of the camera.
3. Press the push button.
4. About a second later, the bone-conduction speaker quietly says the total in English. For example, with a ₱500 and a ₱200 in view: "seven hundred pesos."
5. The device returns to deep sleep on its own.

When the battery gets low, the announcement starts to sound choppy or the device fails to boot. Recharge by plugging the USB-C cable into the XIAO — there's a charging chip onboard.

---

## 11. Troubleshooting

| Symptom | What it means | What to do |
|---|---|---|
| Serial monitor prints `PSRAM init failed` | The big extra memory chip isn't being detected. | Make sure you have the **Sense** version of the board (not the regular XIAO ESP32-S3 — those don't have PSRAM). Re-seat the board if it's on a header. If it persists, the unit may be defective. |
| `Camera init failed` on serial monitor | The board can't talk to its built-in camera. | The camera ribbon connector on the bottom of the XIAO Sense can come loose. Carefully press it back down. If you ever pulled the camera off, the ribbon must be flat with the contacts facing the board. |
| No audio at all | I2S wiring or amp power | Check that VIN on the MAX98357A reads 3.3 V vs GND. Check the 3 audio wires (DIN, BCLK, LRC) match the pin table. Try the amp's GAIN pin briefly to GND for max volume to rule out a quiet bone speaker. |
| Buzzing / static instead of speech | Ground loop or floating GAIN | Make sure the amp's GND, the XIAO's GND, and the battery's negative all share a single point. Don't leave the SD pin floating in noisy environments — tie it to 3V3 if needed. |
| Device reboots when camera fires (`Brownout detector was triggered`) | Battery can't supply the inrush current | Solder the 220 µF capacitor across 3V3 and GND near the XIAO. If the battery is old, replace it. |
| Button doesn't wake the device | Wiring or wrong pin | Confirm one side of the button goes to **D4 (GPIO 5)** and the other to **GND**. Test continuity with a multimeter while pressing it. The firmware uses an internal pull-up; no external resistor is needed. |
| Build fails with `model too large` or `region 'iram' overflowed` | The AI model is bigger than the chip can hold | This shouldn't happen with the small model we ship. If you swapped in a bigger model, retrain at 320×320 instead of 640×640, or fully quantize to int8. |
| `edge-tts: Error: Could not connect to the server` when generating clips | Internet hiccup | Try again — the service is free and occasionally rate-limits. If it persists, the script falls back to a basic synthesized voice. |
| `ffmpeg: command not found` when generating clips | ffmpeg isn't installed | Install ffmpeg per section 4d, or the script will use Python's built-in audio fallback (lower quality but works). |
| Speech sounds slowed-down or chipmunk-fast | Sample rate mismatch | The audio player expects 16 kHz mono 16-bit. If you regenerated clips at a different rate, regenerate them with the default settings. |

---

## 11b. Known build issues

**TFLite Micro inference is gated behind a build flag.** This first version of the firmware ships with `ENABLE_TFLITE_MICRO = 0` (off). When the device wakes and tries to detect bills, `infer_init()` logs a clear message and the device plays the `error.wav` clip and goes back to sleep — i.e., the firmware *runs end to end* (camera, audio, sleep, button), but the actual ML inference is a no-op until you enable it.

Why? Espressif publishes their TFLite Micro port (`esp-tflite-micro`) as an **ESP-IDF component**, not as a registered PlatformIO Arduino library. Several community Arduino-only forks exist but none are reliably compatible with the modern ESP32-S3 toolchain — we tried three (`tflite-micro-arduino-examples`, `chirale/Arduino_TensorFlowLite`, `tanakamasayuki/Arduino_TensorFlowLite_ESP32`) and each broke in a different way. The TFLite C++ code in `firmware/src/inference.cpp` is fully written and preserved verbatim behind the flag — what's missing is just the proper way to *load* the library into PlatformIO.

**To turn inference on**, do one of these:

1. **(Recommended)** Switch `framework` in `platformio.ini` to `arduino, espidf` (mixed mode), add an `idf_component.yml` with `espressif/esp-tflite-micro`, and rebuild with `build_flags += -DENABLE_TFLITE_MICRO=1`. This is well-documented in the Espressif docs and is how serious ESP32 ML projects ship in 2026.
2. Convert the firmware to pure ESP-IDF (no Arduino layer). More work, biggest perf upside.
3. Wait for a stable Arduino-ESP32 TFLite Micro library to land in the PlatformIO registry (check `platformio.org/lib` periodically).

Either way, the rest of the firmware (camera, audio, speech, sleep, button) is fully functional and tested.

---

## 12. Message for Swishy's hardware team (Discord-ready)

> Hey team — two small but important corrections to the BOM before final assembly:
>
> **1) Drop the SYN6288 TTS chip.** Two reasons: (a) it outputs analog audio, but our amplifier (MAX98357A) takes digital I2S input — they're incompatible without an extra ADC stage, and (b) the SYN6288 can't do English well anyway. We're replacing it with pre-recorded English voice clips generated by Microsoft's free neural-voice tool — saved on the chip's built-in flash, played back through the same MAX98357A amp we already have. Studio-quality voice, zero recording effort, and one less component to source.
>
> **2) Add a small tactile push button** (any momentary switch is fine). It connects between D4 / GPIO 5 and GND on the XIAO. This gives the user a clean way to trigger a reading and lets the board sleep at <50 µA between presses, which more than doubles battery life on the 600 mAh pack.
>
> Net change to the BOM: -1 chip, +1 button, ~₱100 cheaper, more reliable, and supports English out of the box. Wiring stays simple — the build guide has the updated diagram. Holler if you want me to walk through it on a call.

---

That's the whole loop. Build it, flash it, hand it to someone who needs it.
