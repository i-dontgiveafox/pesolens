# Multiple-Bill Recognition — What Changed (Plain English)

Date: 2026-07-27

## The short version

PesoLens can now recognize **several bills at once** and announce the **combined
total** — for example, a 500 and a 100 in front of the camera will say
"six hundred pesos." Before, it only ever reported **one** bill per scan.

**We did NOT retrain the AI to do this.** The AI already knew how. We just stopped
the program from throwing away the extra bills.

## Why it was so simple (the important part)

The AI model (called YOLO) is a "detector." A detector's whole job is to find
**every** object in a picture and draw a box around each one. So the model was
*always* finding all the bills in the frame — even back when the device only
announced one.

The reason it only announced one bill was a single on/off switch in our own
program, not a limit of the AI. That switch said: *"after the AI finds the bills,
keep only the strongest one and delete the rest."* We flipped that switch off.

Everything needed to handle multiple bills was already built and just wasn't being
fed:
- the part that **adds up** the bills already added up a list (it just always got
  a list of one),
- the part that **speaks the amount** already speaks any total (it says the
  summed number, not a fixed denomination).

So the change was essentially one line, plus verifying the rest of the chain works.

## What we actually changed

1. **The switch.** In the settings file (`config.h`), `SINGLE_BILL_MODE` was
   changed from `1` (on) to `0` (off). That's the whole feature.

2. **A safety check we confirmed still works.** There's a separate rule that stops
   *one* physical bill from being mistakenly counted twice (if the AI accidentally
   draws two overlapping boxes on the same bill, they cancel down to one). That
   rule stays on, so turning on multi-bill does not risk inflating the total for a
   single bill.

3. **A small log-message fix.** The device's debug text used to label a whole scan
   with one bill's name, which looked wrong once multiple bills were allowed. It
   now prints, e.g., `DETECTED: 2 bills = P600` and a breakdown like
   `1x P500  1x P100`. This only affects the text you see on a computer while
   testing — it does not change what the device says out loud.

4. **We checked the voice clips.** Adding bills can create totals the device never
   had to say before (e.g., 20 + 50 = "seventy pesos"). We confirmed every needed
   number word already has a recording, so nothing is missing.

## How to use / test it

1. Plug in the device (shows up as **COM3**).
2. Open the serial monitor to watch what it's doing.
3. Lay **two or more bills** flat in the camera view — **not overlapping** each
   other (overlapping bills can merge into one).
4. Press the **D6** button. Wait ~3 seconds (aim time) + up to ~60 seconds
   (the AI is slow on this small chip — separate known issue).
5. It should say the **combined total** out loud, and the log shows
   `[result] <total> pesos from <N> bills`.

## Honest limitations (so nobody is surprised)

- **Speed:** each scan still takes roughly a minute on this chip. Multi-bill does
  not make it slower, but it's not instant.
- **Don't overlap the bills.** Overlapping bills can be merged and undercounted.
- **This exact multi-bill behavior hasn't been tested with real bills yet** — the
  code is confirmed correct and the device boots fine, but the first real
  two-bill press still needs to be done.
