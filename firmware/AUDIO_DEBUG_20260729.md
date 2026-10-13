# Audio Debug Session — 2026-07-29

Follow-on to `AUDIO_DEBUG_20260728.md`. Investigating "audio stopped working after
the multi-bill change."

## Bottom line

**Audio works in the full firmware build.** A boot-time self-test beep was added and
confirmed audible by the user, with camera and TFLM initialized — not the isolated
`env:audio_probe` build. This is the first time audio has ever been confirmed in the
real firmware.

**The multi-bill change was never the cause.** Two independent reasons:

1. The change is `SINGLE_BILL_MODE 1 -> 0`, which compiles out one `kept.resize(1)`
   block in `inference.cpp:385-393`. Nothing downstream is touched, and *both*
   branches in `scan_once` still call audio — `play_announcement()` when `total > 0`,
   `audio_play_clip("error")` otherwise. `audio.cpp` / `speech.cpp` / `audio.h`
   unchanged since May.
2. There was no working "before" state to regress from. `PROGRESS.md:51` (in the
   2026-07-27 current-state section) and `PROGRESS.md:387` both record that speaker
   output was **never physically confirmed**. The premise of the bug report is wrong.

## What was changed this session

| Change | File | Why |
|---|---|---|
| Boot audio self-test beep | `src/main.cpp` (after `audio_init()`) | Proves the I2S→amp→speaker chain in *this* build with no button press and no 60-70 s inference. Isolates "audio broken" from "scan never reaches audio". |
| IDF console UART0 → USB-Serial-JTAG | `sdkconfig.defaults` (already written 07-28, never applied) | UART0 on ESP32-S3 is GPIO43/44 = `CAPTURE_BUTTON_PIN` and `I2S_LRC`. Verified against IDF's own `components/soc/esp32s3/include/soc/uart_pins.h`: `U0RXD_GPIO_NUM 44`, `U0TXD_GPIO_NUM 43`. |
| Task WDT corrected | `sdkconfig.defaults` | See below — the previously "pending" fix was invalid. |

### Correction: the pending WDT 60 → 180 change was impossible

`AUDIO_DEBUG_20260728.md` listed `CONFIG_ESP_TASK_WDT_TIMEOUT_S` 60 → 180 as a pending
fix. **It cannot work.** In ESP-IDF 4.4 (`components/esp_system/Kconfig:408-412`) the
symbol is declared `range 1 60`. Setting 180 is silently rejected and Kconfig falls
back to its `default 5` — a *5 second* watchdog, far worse than the 60 s it replaced.
This was observed empirically: the first regenerated sdkconfig came out with
`CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`.

Correct approach, now in `sdkconfig.defaults`: cap at 60 (the max) and stop the spam at
its source by unsubscribing the idle tasks that a 60-70 s blocking scan starves:

```
CONFIG_ESP_TASK_WDT=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=60
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n
```

## Verified boot log (2026-07-29)

`logs/boot-audio-selftest-20260729.log`:

```
[audio] init ok (16 kHz, 16-bit, mono)
[audio] boot self-test: playing beep
[audio] playing beep (200ms)
[audio] boot self-test done
[scan] manual mode - waiting for D6/GPIO43 press
[ready] D5/GPIO6 LED on - ready for capture
```

User confirmed the beep was **audible**.

Note the timing change: `env:audio_probe` measured `beep` at 67-83 ms; this build
reports **200 ms**, the clip's true full duration (6,400 PCM bytes / 2 / 16 kHz).
`i2s_write` now blocks until the whole clip is clocked out rather than returning while
the DMA tail drains. Benign, but it is a real behavioural difference from the probe.

## Landmine: stale per-env sdkconfigs

PlatformIO generates `sdkconfig.<env>` on first build and **reuses it forever**;
`sdkconfig.defaults` only seeds that first generation. Editing `sdkconfig.defaults`
does nothing to an env that already has a generated file. To apply changes you must
delete `sdkconfig.<env>` and rebuild.

Audit as of this session — only the two regenerated envs have the fixes:

| env | console |
|---|---|
| `seeed_xiao_esp32s3_tflm` | USB-Serial-JTAG (fixed) |
| `seeed_xiao_esp32s3_tflm_button_capture` | USB-Serial-JTAG (fixed) |
| `seeed_xiao_esp32s3_tflm_auto_capture_dump_8s` | UART0 (stale) |
| `seeed_xiao_esp32s3_tflm_auto_run_d6_led` | UART0 (stale) |
| `seeed_xiao_esp32s3_tflm_button_capture_dump` | UART0 (stale) |
| `seeed_xiao_esp32s3_tflm_detect_dump` | UART0 (stale) |
| `seeed_xiao_esp32s3_tflm_frame_dump` | UART0 (stale) |

Originals backed up in `backups/sdkconfig-pre-console-fix-20260729/`.

**`auto_run_d6_led` is the worst case and it is the standalone deployment build.** It
sets `READY_LED_PIN=43` and drives GPIO43 as a push-pull `OUTPUT`, while UART0 TX is on
that same pad. Unlike the button build — where `pinMode(43, INPUT_PULLUP)` takes the pin
cleanly — here two drivers contend for the pad permanently. Regenerate this env's
sdkconfig before ever shipping it.

## Still open

- **D6 button still never observed.** A 200 s capture on 2026-07-29 recorded 0 bytes.
  In manual mode `loop()` prints nothing while idle, so an empty log is indistinguishable
  between "no press registered" and "no press attempted" — it was not confirmed whether a
  press was made during the window. Five sessions, still no D6 press on the wire.
- **Full scan → audio path still never observed end to end.** Boot audio is proven;
  post-inference audio is not.
- **Which env is on the friend's unit is unknown.** If it is `auto_run_d6_led` rather
  than `button_capture`, the failing firmware is not the one being debugged on COM3.

## Unrelated finding: speech clips are much quieter than the tones

Measured across `data/*.wav` (all correctly 16 kHz / 16-bit / mono, all 35 clips that
`compose_english()` can emit are present — no missing-file gap):

| clip type | peak | RMS |
|---|---|---|
| `beep.wav` / `error.wav` (pure tones) | ~60% | **~13,700** |
| every speech clip | 40-80% | **~1,400-2,800** |

Similar peaks, but 14-20 dB less average power, because a tone is dense and speech is
sparse. Bone-conduction transducers respond to average energy. This is *not* the cause of
total silence — the ready beep uses `beep.wav` and was equally silent — but once the scan
path is working, expect the spoken words to be substantially harder to hear than the beep.
Fix by normalizing/compressing the speech clips, not by raw gain: their peaks are already
at 40-80%, so linear gain will clip.

## Separate regression: multi-bill inflates totals

Not an audio issue, but real. With `SINGLE_BILL_MODE 1`, spurious extra boxes were
silently discarded by `kept.resize(1)`. With it off, every surviving box adds its
denomination to the announced total, so false detections now corrupt the amount.
`PROGRESS.md:200` already flagged the duplicate-P50 → announced-P100 case.
`CLASS_AGNOSTIC_NMS` only suppresses *overlapping* boxes; it does nothing about
spatially separate false positives.
