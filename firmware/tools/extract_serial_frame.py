#!/usr/bin/env python3
"""Extract a JPEG frame dumped by PESOS_DUMP_FIRST_FRAME serial logs."""

from __future__ import annotations

import argparse
import base64
from pathlib import Path


BEGIN = "[frame] JPEG_BASE64_BEGIN"
END = "[frame] JPEG_BASE64_END"


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract a serial-dumped JPEG frame.")
    parser.add_argument("log", type=Path)
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("logs/camera-frame-latest.jpg"),
        help="Output JPEG path.",
    )
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="ignore")
    start = text.find(BEGIN)
    if start < 0:
        raise SystemExit(f"missing marker: {BEGIN}")
    start = text.find("\n", start)
    if start < 0:
        raise SystemExit("frame marker has no following data")
    end = text.find(END, start)
    if end < 0:
        raise SystemExit(f"missing marker: {END}")

    payload = "".join(
        line.strip()
        for line in text[start:end].splitlines()
        if line.strip() and not line.startswith("[")
    )
    data = base64.b64decode(payload, validate=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"wrote {len(data)} bytes to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
