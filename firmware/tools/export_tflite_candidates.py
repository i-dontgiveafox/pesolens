#!/usr/bin/env python3
"""Export PesoLens TFLite candidates at smaller input sizes."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from ultralytics import YOLO


USER_ROOT = Path.home()
DEFAULT_MODEL = USER_ROOT / "PesoLens-fixed-2026-05-12" / "PesoLens" / "pesolens_best.pt"
DEFAULT_DATA = (
    USER_ROOT
    / "PesoLens-fixed-2026-05-12"
    / "PesoLens"
    / "pesolens-roboflow"
    / "data.yaml"
)
DEFAULT_OUT = Path(__file__).resolve().parent / "tflite_exports"


def find_tflites(root: Path) -> list[Path]:
    if not root.exists():
        return []
    return sorted(root.rglob("*.tflite"))


def copy_outputs(export_root: Path, dest_dir: Path) -> list[Path]:
    copied: list[Path] = []
    for src in find_tflites(export_root):
        dest = dest_dir / src.name
        if dest.exists() and dest.resolve() == src.resolve():
            copied.append(dest)
            continue
        shutil.copy2(src, dest)
        copied.append(dest)
    return copied


def export_candidate(model_path: Path, data_yaml: Path, out_dir: Path, imgsz: int) -> list[Path]:
    name = f"pesolens_best_imgsz{imgsz}_int8"
    export_root = out_dir / name
    export_root.mkdir(parents=True, exist_ok=True)

    print(f"\n=== exporting imgsz={imgsz} ===", flush=True)
    print(f"model: {model_path}", flush=True)
    print(f"data : {data_yaml}", flush=True)
    print(f"out  : {export_root}", flush=True)

    model = YOLO(str(model_path))
    result = model.export(
        format="tflite",
        int8=True,
        imgsz=imgsz,
        data=str(data_yaml),
        project=str(out_dir),
        name=name,
        exist_ok=True,
        verbose=True,
    )
    print(f"export result: {result}", flush=True)

    copied = copy_outputs(export_root, export_root)
    if not copied:
        # Some Ultralytics versions write next to the PT file even when project/name
        # is supplied. Copy only freshly named candidates if they exist.
        pt_parent = model_path.parent
        copied = [
            p
            for p in find_tflites(pt_parent)
            if f"imgsz{imgsz}" in p.name or p.parent.name == name
        ]

    for path in copied:
        print(f"candidate: {path} ({path.stat().st_size:,} bytes)", flush=True)
    return copied


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    parser.add_argument("--data", type=Path, default=DEFAULT_DATA)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--sizes", type=int, nargs="+", default=[96, 128, 160])
    args = parser.parse_args()

    if not args.model.is_file():
        raise FileNotFoundError(args.model)
    if not args.data.is_file():
        raise FileNotFoundError(args.data)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    all_candidates: list[Path] = []
    for size in args.sizes:
        all_candidates.extend(export_candidate(args.model, args.data, args.out_dir, size))

    print("\n=== summary ===", flush=True)
    unique = sorted({p.resolve() for p in all_candidates})
    for path in unique:
        print(f"{path} | {path.stat().st_size:,} bytes", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
