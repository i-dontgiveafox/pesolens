# pesos

AI-powered eyeglasses for visually-impaired Filipino users. Press a button on the side of the glasses, and a small bone-conduction speaker quietly says the total of any Philippine peso bills the camera can see — in English, fully offline, no internet, no cloud.

## Where to start

**Read [docs/SETUP_GUIDE.md](docs/SETUP_GUIDE.md) first.** It walks through everything from buying the parts to flashing the firmware to using the finished glasses, in plain English with no jargon.

## Layout

| Folder | What's inside |
|---|---|
| [`firmware/`](firmware/) | The PlatformIO project that runs on the glasses (the ESP32-S3 chip). C++ source, build config, audio data folder. |
| [`docs/`](docs/) | All written documentation. Start with `SETUP_GUIDE.md`. Other files: `HARDWARE.md` (parts list + wiring), `DEPLOYMENT.md` (build/flash commands), `AUDIO_CLIPS.md` (list of voice clips). |

## What's not in this zip

The training side (datasets, model checkpoints, conversion notebooks) lives separately. This bundle contains everything you need to **build and flash** the glasses; if you ever need to re-train or tune the model, the training scripts and 5,100-image merged Roboflow dataset are kept in the parent project.

## Status

**Firmware compiles cleanly** (`pio run` succeeds, ~390 KB binary, plenty of headroom). On-device TFLite Micro inference is gated behind a flag — see "Known build issues" in `docs/SETUP_GUIDE.md` for the exact next step (switch to `arduino, espidf` mixed framework + add `esp-tflite-micro` as an IDF component). Everything else (camera capture, audio playback, button wake, deep sleep, English speech composition) is fully implemented and ready to flash.
