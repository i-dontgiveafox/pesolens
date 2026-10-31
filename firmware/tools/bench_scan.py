#!/usr/bin/env python3
"""Bench test: trigger one scan over USB serial and save the log + JPEG.

Works with the button_capture / button_capture_dump firmware (send 's' to scan).
Usage:
    python tools/bench_scan.py COM3 --label p100_25cm
    python tools/bench_scan.py COM3 --label boot --no-trigger   # just listen (e.g. after a replug)

Writes logs/bench/<timestamp>_<label>.log and .jpg (dump firmware only), then
prints the result lines.
"""

from __future__ import annotations

import argparse
import base64
import re
import sys
import time
from pathlib import Path

import serial

BEGIN = "[frame] JPEG_BASE64_BEGIN"
END = "[frame] JPEG_BASE64_END"
DONE_MARKERS = ("[scan] idle - waiting for next trigger",)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--label", default="scan")
    ap.add_argument("--timeout", type=float, default=150.0)
    ap.add_argument("--no-trigger", action="store_true")
    ap.add_argument("--cmd", default="s", choices=["s", "a", "c"],
                    help="s=full scan, a=speak test amount, c=capture-only frame dump")
    args = ap.parse_args()

    out_dir = Path(__file__).resolve().parent.parent / "logs" / "bench"
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = time.strftime("%Y%m%d-%H%M%S") + "_" + re.sub(r"[^\w.-]", "_", args.label)
    log_path = out_dir / f"{stem}.log"

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = 115200
    ser.timeout = 0.2
    ser.dtr = False  # don't reset the chip on open
    ser.rts = False
    ser.open()

    buf = bytearray()
    t0 = time.monotonic()
    if not args.no_trigger:
        time.sleep(0.3)
        ser.reset_input_buffer()
        ser.write(args.cmd.encode())
        ser.flush()
    done_at = None
    while time.monotonic() - t0 < args.timeout:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk
            sys.stdout.write(chunk.decode("utf-8", "replace").replace(
                "\r", "") if b"BASE64" not in chunk and len(chunk) < 400 else "")
            sys.stdout.flush()
        text = buf.decode("utf-8", "ignore")
        if not args.no_trigger:
            # Ignore output from before the board acknowledged our command.
            ack = max(text.rfind("[manual] serial 's'"), text.rfind("[bench] audio test"),
                      text.rfind("[bench] capture-only test"))
            text = text[ack:] if ack >= 0 else ""
        if done_at is None and any(m in text for m in DONE_MARKERS):
            done_at = time.monotonic()
        if done_at is not None and time.monotonic() - done_at > 0.5:
            break
    ser.close()

    text = buf.decode("utf-8", "ignore").replace("\r", "")
    log_path.write_text(text, encoding="utf-8")
    print(f"\n[bench] log: {log_path}")

    s = text.rfind(BEGIN)
    e = text.find(END, s + 1) if s >= 0 else -1
    if s >= 0 and e > 0:
        payload = "".join(
            ln.strip() for ln in text[text.find("\n", s):e].splitlines()
            if ln.strip() and not ln.startswith("[")
        )
        jpg = out_dir / f"{stem}.jpg"
        jpg.write_bytes(base64.b64decode(payload))
        print(f"[bench] frame: {jpg}")

    for ln in text.splitlines():
        if ln.startswith(("[detect]", "[result]", ">>>", "[speak]", "[time]", "[err]", "[ready] D", "[cam]", "[bench]", "[audio]")):
            print("  " + ln)
    if done_at is None:
        print("[bench] WARNING: scan did not finish before timeout")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
