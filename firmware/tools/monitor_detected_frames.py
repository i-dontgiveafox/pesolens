#!/usr/bin/env python3
"""Monitor PesoLens serial output and save JPEG frames dumped after detections."""

from __future__ import annotations

import argparse
import base64
from datetime import datetime
from pathlib import Path
import re
import sys
import time

import serial
from serial import SerialException


BEGIN = "[frame] JPEG_BASE64_BEGIN"
END = "[frame] JPEG_BASE64_END"
RESULT_RE = re.compile(r"\[result\]\s+(\d+)\s+pesos from\s+(\d+)\s+bills")
SCAN_START = "[scan] capture start"


def label_for_result(total: int | None, bills: int | None) -> str:
    if total is None or bills is None:
        return "unknown"
    return f"P{total}-{bills}bill{'s' if bills != 1 else ''}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Read PesoLens serial logs, mirror useful lines, and save every "
            "JPEG frame emitted between JPEG_BASE64 markers."
        )
    )
    parser.add_argument("--port", default="COM3", help="Serial port, default COM3.")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("logs/detections"),
        help="Directory where JPEG captures are written.",
    )
    parser.add_argument(
        "--log",
        type=Path,
        default=None,
        help="Full serial log path. Defaults to logs/detection-monitor-<timestamp>.log.",
    )
    parser.add_argument(
        "--seconds",
        type=float,
        default=None,
        help="Optional bounded run time. Omit to run until Ctrl+C.",
    )
    parser.add_argument(
        "--reset",
        action="store_true",
        help="Toggle RTS after opening the port to reboot the board.",
    )
    args = parser.parse_args()

    run_stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir: Path = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)
    log_path = args.log or Path("logs") / f"detection-monitor-{run_stamp}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)

    print(
        f"[host] listening on {args.port} at {args.baud}; "
        f"saving captures to {out_dir}",
        flush=True,
    )
    print(f"[host] full serial log: {log_path}", flush=True)

    frame_lines: list[str] = []
    in_frame = False
    frame_index = 0
    last_total: int | None = None
    last_bills: int | None = None
    deadline = time.monotonic() + args.seconds if args.seconds is not None else None

    try:
        with serial.Serial(args.port, args.baud, timeout=0.2) as ser, log_path.open(
            "w", encoding="utf-8", errors="replace"
        ) as log:
            if args.reset:
                ser.dtr = False
                ser.rts = True
                time.sleep(0.15)
                ser.rts = False
                time.sleep(0.2)

            while deadline is None or time.monotonic() < deadline:
                raw = ser.read_until(b"\n", 4096)
                if not raw:
                    continue

                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                log.write(line + "\n")
                log.flush()

                if BEGIN in line:
                    in_frame = True
                    frame_lines = []
                    print("[host] receiving JPEG frame...", flush=True)
                    continue

                if END in line and in_frame:
                    payload = "".join(frame_lines)
                    try:
                        data = base64.b64decode(payload, validate=True)
                    except Exception as exc:
                        print(f"[host] ERROR: frame decode failed: {exc}", flush=True)
                    else:
                        frame_index += 1
                        capture_stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
                        label = label_for_result(last_total, last_bills)
                        out_path = out_dir / f"capture-{capture_stamp}-{label}-{frame_index:03d}.jpg"
                        out_path.write_bytes(data)
                        print(f"[host] saved {out_path} ({len(data)} bytes)", flush=True)
                    in_frame = False
                    frame_lines = []
                    continue

                if in_frame:
                    stripped = line.strip()
                    if stripped and not stripped.startswith("["):
                        frame_lines.append(stripped)
                    continue

                if SCAN_START in line:
                    # Old dump firmware emitted a frame before inference. Clear
                    # the prior result so that frame is "unknown", not falsely
                    # labelled with the preceding scan's denomination.
                    last_total = None
                    last_bills = None

                result_match = RESULT_RE.search(line)
                if result_match:
                    last_total = int(result_match.group(1))
                    last_bills = int(result_match.group(2))

                print(line, flush=True)

    except KeyboardInterrupt:
        print("\n[host] stopped by user", flush=True)
    except SerialException as exc:
        print(f"[host] serial error: {exc}", file=sys.stderr, flush=True)
        return 1

    print("[host] monitor closed", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
