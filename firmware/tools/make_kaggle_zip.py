#!/usr/bin/env python3
"""Create a Kaggle-compatible ZIP with forward-slash archive paths."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Dataset folder to zip")
    parser.add_argument("--output", type=Path, required=True, help="Output .zip path")
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()
    if not source.is_dir():
        raise NotADirectoryError(source)
    output.parent.mkdir(parents=True, exist_ok=True)

    root_name = source.name
    files = [p for p in source.rglob("*") if p.is_file()]
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for path in files:
            rel = path.relative_to(source).as_posix()
            zf.write(path, f"{root_name}/{rel}")

    with zipfile.ZipFile(output, "r") as zf:
        bad = [name for name in zf.namelist() if "\\" in name]
    if bad:
        raise RuntimeError(f"Backslash paths found in ZIP: {bad[:5]}")

    print(f"created: {output}")
    print(f"files: {len(files):,}")
    print(f"size: {output.stat().st_size:,} bytes")
    print("archive paths: OK, forward slashes only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
