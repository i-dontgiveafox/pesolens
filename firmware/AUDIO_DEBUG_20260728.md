# Audio Debug Session — 2026-07-28

Investigating the report: "after we updated the code, the audio no longer works."

## Bottom line

**The audio hardware and the audio firmware both work.** A minimal audio-only build
plays clips audibly through the existing amp and speaker. No fault was found in any
audio code, and no audio code had been changed.

**The failing path was never actually observed.** Despite ~20 minutes of serial
recording across four sessions, not one D6 capture cycle was ever captured, so the
scan → inference → audio path has still never been logged end to end.

## Board state at end of session

| Item | State |
|---|---|
| Port | `COM3`, `VID:PID 303A:1001`, MAC `e0:72:a1:fa:38:3c` |
| App firmware | `seeed_xiao_esp32s3_tflm_button_capture`, reflashed, hash verified |
| SPIFFS | Reuploaded — all 36 WAV clips written to `0x520000`, hash verified |
| Boot | Healthy: camera OV3660, `[infer] ready`, `[audio] init ok`, reaches ready state |

Upload works fine with a plain `--target upload`. The old "upload is blocked, use
manual ROM download mode" note is stale — esptool's default reset works.

## Confirmed working (by direct evidence, not inference)

Flashing `env:audio_probe` and monitoring produced:

```
[audio_probe] beep 15
[audio] playing beep (67ms)
[audio_probe] beep 16
[audio] playing beep (83ms)
```

The user confirmed this was **audible**. That single test proves the entire chain:

- MAX98357A amplifier, its power, and its `SD`/shutdown pin
- Bone-conduction speaker and its wiring
- Shared ground between amp and XIAO
- I2S wiring on DIN `GPIO8`, BCLK `GPIO7`, LRC `GPIO44`
- SPIFFS mount and the WAV clips on it
- `audio_init()`, `audio_play_clip()`, and the mono→stereo conversion in `audio.cpp`

The reported timing corroborates it rather than just echoing a log line.
`beep.wav` is 6,444 bytes → 6,400 bytes PCM → 3,200 samples @ 16 kHz = **200 ms**.
DMA capacity is `dma_buf_count 8 × dma_buf_len 256` = 2,048 frames = **128 ms**.
So `i2s_write` should block ~72 ms. Measured 67–83 ms. The samples are genuinely
being clocked out of the peripheral at the correct rate.

## Ruled out

| Theory | How it was killed |
|---|---|
| A code regression broke audio | `audio.cpp`/`speech.cpp`/`audio.h` unchanged since May. Diff of `main.cpp` vs `backups/pre-debug-logging-20260614/` is only logging + the multi-bill banner. |
| `SINGLE_BILL_MODE 1→0` broke it | Only functional change on 2026-07-27; does not touch the audio path. |
| SPIFFS clips lost by app-only flashing | Clips verified present and streaming. `--target upload` never touches `0x520000`. |
| MAX98357A `SD` pin held in shutdown | Probe beeps audibly, so the amp is enabled. |
| Floating ground / loose wiring | Same. |
| `audio_init()` failing in the full build | `[audio] init ok` in every boot log, after camera and TFLM init. |
| Board resetting during inference | User observed the pre-audio LED blink sequence, which runs *after* inference. |
| Task watchdog rebooting the board | `CONFIG_ESP_TASK_WDT_PANIC is not set` — it warns, never reboots. |
| Camera/audio GPIO collision | Camera uses 10/39/40/48/11/12/14/16/18/17/15/38/47/13. Audio uses 8/7/44. No overlap. |
| Camera/audio clock collision | Camera XCLK is `LEDC_CHANNEL_0`/`LEDC_TIMER_0` on GPIO10; independent of I2S. |
| UART0 console stealing I2S LRC | Real pin overlap (UART0 = GPIO43/44), but `i2s_set_pin()` re-muxes GPIO44 afterwards and wins. Hygiene issue only. |

## Still open

The D6 button never registered a press during any recording window. Four sessions
totalling ~20 minutes of monitoring produced **0 bytes**. In manual mode the firmware
prints nothing while idle (`loop()` polls with `delay(10)`), so an empty log is
exactly what "no press registered" looks like — it is not a capture failure.

The user reports a real button is wired between D6/GPIO43 and GND, and at one point
observed a 3-blink sequence followed by silence. That observation is the strongest
remaining clue but could not be reproduced on the wire.

Note `GPIO[43]| InputEn: 1| OutputEn: 0| Pullup: 1` appears in the boot log, so the
pin is configured correctly as an active-low input.

## Next steps, in order

1. **Prove whether D6 presses reach the chip.** Flash a probe that prints
   `digitalRead(43)` continuously. This is the actual blocker — everything else is
   downstream of a trigger that may never fire.
2. **Bypass the button entirely.** `env:seeed_xiao_esp32s3_tflm` is the same firmware
   without `-DPESOS_MANUAL_CAPTURE`; it scans on a loop with no button and no LED.
   This is the fastest way to get a complete scan cycle logged.
3. **Capture the scan log and read the playback timing.** The decisive number is the
   ms figure on `[audio] playing ...`:
   - ~67–83 ms like the probe → I2S is clocking correctly; sound *is* being produced,
     so investigate amplitude (speech/error clips may be far quieter than `beep.wav`).
   - ~0 ms → `i2s_write` returning instantly, DMA not draining, I2S clock stopped.
   - `missing clip` → filename mismatch on the announcement path that the beep never
     exercises.

## Gotcha discovered this session

**Editing `sdkconfig.defaults` has no effect once `sdkconfig.<env>` exists.**
PlatformIO's ESP-IDF integration generates `sdkconfig.<env>` on first build and reuses
it forever; `sdkconfig.defaults` only seeds that initial generation. Verified:
`sdkconfig.defaults` modified 2026-07-28, `sdkconfig.seeed_xiao_esp32s3_tflm_button_capture`
still dated 2026-05-30 after a successful rebuild and flash.

To apply changes, delete the generated `sdkconfig.<env>` and rebuild — that forces a
slow full rebuild of IDF + Arduino + TFLM.

Also note `board_build.sdkconfig_options` in `platformio.ini` (lines 28, 79) is not a
recognized PlatformIO option and is silently ignored.

### Pending, not yet applied

These edits are in `sdkconfig.defaults` but are **not in the flashed binary**:

- `CONFIG_ESP_TASK_WDT_TIMEOUT_S` 60 → 180. The 320px model takes 60–70 s per scan and
  exceeds the old 60 s timeout, so the watchdog fires on every capture. Panic is off so
  it never reboots, but the warnings bury real failures in the log.
- IDF console moved off UART0 to USB-Serial-JTAG, so GPIO43/44 belong solely to the D6
  button and I2S LRC. Hygiene, not a fix.

## Unrelated: 2.4 GHz keyboard interference

Plugging in the ESP32-S3 disrupts an AULA F98 PRO V3 on a 2.4 GHz dongle.

This is **not** the ESP32's radio. `firmware/src` contains no `WiFi`, `esp_wifi`, `BLE`,
`btStart`, or `esp_bt` references — the radio is never started. It is USB 3.0 broadband
RF noise in the 2.4–2.5 GHz band, a well-documented effect of SuperSpeed signalling.

Mitigations, cheapest first: move the dongle to a USB 2.0 port or onto an extension
cable away from the PC; put the XIAO on USB 2.0 too; keep the XIAO cable physically
separated from the dongle; or switch the F98 PRO to Bluetooth/wired during firmware work.
