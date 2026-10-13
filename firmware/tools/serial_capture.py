#!/usr/bin/env python3
"""Capture serial output for a fixed duration."""

from __future__ import annotations

import argparse
import sys
import time

import serial
from serial import SerialException


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture serial output briefly.")
    parser.add_argument("port", help="Serial port, for example COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=30.0)
    args = parser.parse_args()

    print(
        f"[serial_capture] listening on {args.port} at {args.baud} baud "
        f"for {args.seconds:g} seconds",
        flush=True,
    )

    byte_count = 0
    try:
        with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
            deadline = time.monotonic() + args.seconds
            while time.monotonic() < deadline:
                try:
                    chunk = ser.read(512)
                except SerialException as exc:
                    print(f"\n[serial_capture] serial read stopped: {exc}", flush=True)
                    break
                if chunk:
                    byte_count += len(chunk)
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
    except SerialException as exc:
        print(f"[serial_capture] open failed: {exc}", flush=True)
        return 1

    print(f"\n[serial_capture] closed; bytes={byte_count}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
