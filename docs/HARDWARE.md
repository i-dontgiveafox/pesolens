# Hardware — pesos AI-glasses

Concise BOM, the corrections we made vs. the original spec, and the wiring diagram. For the full beginner walkthrough see `SETUP_GUIDE.md`.

## Bill of materials

| # | Part | Required? | Notes |
|---|---|---|---|
| 1 | Seeed XIAO ESP32-S3 **Sense** | Required | The "Sense" variant has an OV2640 or newer OV3660 camera + 8 MB PSRAM + 8 MB flash on board. The non-Sense XIAO ESP32-S3 does **not** work — no PSRAM. |
| 2 | MAX98357A I2S audio amplifier | Required | Adafruit #3006 or any clone. Default 9 dB gain is fine for a bone-conduction transducer. |
| 3 | Bone-conduction speaker, 8 Ω | Required | Small (~2-3 cm). Adafruit #1674 works well. |
| 4 | Tactile push button (momentary) | Required | Any normally-open switch. No external resistor — the firmware uses the ESP32-S3 internal pull-up. |
| 5 | 3.7 V 600 mAh LiPo + JST-PH 1.25 mm | Required | Sized for button-trigger duty cycle (~1.2 days typical use). |
| 6 | 220 µF electrolytic capacitor across 3V3/GND | Nice-to-have | Suppresses the brownout that can happen on inrush when the camera fires. |
| 7 | Eyeglass frame, hookup wire, heatshrink, hot glue | Required | Mechanical assembly. |

## BOM corrections vs. original spec

| Component | Action | Reason |
|---|---|---|
| **SYN6288 TTS chip** | **Remove** | (a) Outputs analog audio — incompatible with MAX98357A's I2S input. (b) Cannot speak English with acceptable quality. Replaced by pre-generated English WAV clips played from SPIFFS through the existing MAX98357A. |
| **Tactile push button** | **Add** | Required for the wake-from-deep-sleep trigger pattern. Without it, the device must stay always-on and the 600 mAh battery doesn't last a useful day. |

## Wiring

```
                       Seeed XIAO ESP32-S3 Sense
                       +-------------------------+
                       |                         |
        D1  (GPIO 2) --|---> MAX98357A DIN       |
        D2  (GPIO 3) --|---> MAX98357A BCLK      |
        D3  (GPIO 4) --|---> MAX98357A LRC       |
        D4  (GPIO 5) --|---> push button → GND   |
                       |                         |
        3V3          --|---> MAX98357A VIN       |
        GND          --|---> shared ground       |
                       |                         |
        BAT+ / BAT-  --|---> 600 mAh LiPo (JST)  |
                       |                         |
        camera ribbon  | (onboard, no wiring)    |
                       +-------------------------+

        MAX98357A speaker terminals → bone-conduction transducer (+/-)
```

## Pin map (canonical)

| XIAO pin | GPIO | Function |
|---|---|---|
| D1 | 2 | I2S data out (DIN to amp) |
| D2 | 3 | I2S bit clock (BCLK to amp) |
| D3 | 4 | I2S word/LR clock (LRC to amp) |
| D4 | 5 | Button input (active low, internal pull-up, EXT0 wake source) |
| 3V3 | — | Amp VIN |
| GND | — | Shared ground |

## Camera (built-in) pin map

The XIAO Sense uses the `CAMERA_MODEL_XIAO_ESP32S3` preset. These pins are wired internally on the board — listed for reference only:

| Signal | GPIO |
|---|---|
| XCLK | 10 |
| PCLK | 13 |
| VSYNC | 38 |
| HREF | 47 |
| SIOD | 40 |
| SIOC | 39 |
| Y2..Y9 | 15, 17, 18, 16, 14, 12, 11, 48 |
| PWDN / RESET | not connected |

## Power budget (rough)

| Phase | Current | Duration |
|---|---|---|
| Deep sleep | ~8 µA | most of the time |
| Wake + camera + inference + audio | ~120 mA | ~1.5 s per press |

At 200 button presses per day on a 600 mAh battery: more than a day of headroom.

## Mechanical

Mount the XIAO + camera on the temple (camera lens forward, near the hinge). Run the audio wires along the temple to the bone speaker, which sits in front of/against the bone behind the ear. Hot glue or epoxy is fine. The button can be glued anywhere accessible — the bottom of the temple is the typical spot.
