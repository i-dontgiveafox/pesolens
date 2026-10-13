"""PesoLens — detect Philippine peso banknotes in an image and total them up.

Usage:
    python predict.py                              # uses test_bill.jpg
    python predict.py path/to/photo.jpg
    python predict.py path/to/photo.jpg --conf 0.5
    python predict.py path/to/folder --conf 0.3 --out my_results
    python predict.py path/to/photo.jpg --device 0   # use a CUDA GPU if available

Outputs:
    - Console: per-bill detections + total peso amount
    - On disk: annotated image(s) saved into the chosen output folder
"""

import argparse
import sys
from pathlib import Path

import cv2
from ultralytics import YOLO

DEFAULT_MODEL = Path(__file__).with_name("pesolens_best.pt")
DEFAULT_SOURCE = Path(__file__).with_name("test_bill.jpg")
DEFAULT_OUT = Path(__file__).with_name("predict_results")
DEFAULT_CONF = 0.25
DEFAULT_IOU = 0.3
DEFAULT_DEVICE = "cpu"
IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}
EXPECTED_CLASSES = {"20", "50", "100", "500", "1000"}


def collect_images(source: Path) -> list[Path]:
    if source.is_file():
        return [source]
    if source.is_dir():
        return sorted(p for p in source.iterdir() if p.suffix.lower() in IMAGE_EXTS)
    raise FileNotFoundError(f"Source not found: {source}")


def predict_one(
    model: YOLO,
    image_path: Path,
    conf: float,
    iou: float,
    device: str,
    out_dir: Path,
) -> dict:
    results = model.predict(
        source=str(image_path),
        conf=conf,
        iou=iou,
        device=device,
        verbose=False,
    )
    result = results[0]
    boxes = result.boxes

    bills = []
    for box in boxes:
        denom = int(model.names[int(box.cls[0])])
        confidence = float(box.conf[0])
        bills.append({"denom": denom, "confidence": confidence})

    total = sum(b["denom"] for b in bills)
    by_denom = {}
    for b in bills:
        by_denom.setdefault(b["denom"], []).append(b["confidence"])

    print(f"\n{image_path.name}")
    print("-" * len(image_path.name))
    if not bills:
        print("  No bills detected.")
        print("  Try: better lighting, sharper focus, or a lower --conf threshold.")
    else:
        for denom in sorted(by_denom.keys(), reverse=True):
            confs = by_denom[denom]
            avg = sum(confs) / len(confs)
            print(f"  {len(confs)} x P{denom}  (avg confidence {avg:.0%})")
        print(f"  Total: P{total}")

    annotated = result.plot()
    out_path = out_dir / image_path.name
    cv2.imwrite(str(out_path), annotated)

    return {"image": image_path.name, "bills": bills, "total": total, "annotated": out_path}


def main():
    parser = argparse.ArgumentParser(description="PesoLens — count Philippine peso bills in a photo.")
    parser.add_argument("source", nargs="?", default=str(DEFAULT_SOURCE),
                        help="Image file or folder of images (default: test_bill.jpg)")
    parser.add_argument("--model", default=str(DEFAULT_MODEL),
                        help=f"Path to YOLO weights (default: {DEFAULT_MODEL.name})")
    parser.add_argument("--conf", type=float, default=DEFAULT_CONF,
                        help=f"Min detection confidence 0-1 (default: {DEFAULT_CONF})")
    parser.add_argument("--iou", type=float, default=DEFAULT_IOU,
                        help=f"NMS overlap threshold (default: {DEFAULT_IOU})")
    parser.add_argument("--device", default=DEFAULT_DEVICE,
                        help=f"Ultralytics device, e.g. cpu or 0 for CUDA GPU (default: {DEFAULT_DEVICE})")
    parser.add_argument("--out", default=str(DEFAULT_OUT),
                        help=f"Folder to save annotated images (default: {DEFAULT_OUT.name})")
    args = parser.parse_args()

    model_path = Path(args.model)
    if not model_path.exists():
        sys.exit(f"Model file not found: {model_path}")

    source = Path(args.source)
    images = collect_images(source)
    if not images:
        sys.exit(f"No images found at: {source}")

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    model = YOLO(str(model_path))
    actual_classes = set(map(str, model.names.values()))
    if actual_classes != EXPECTED_CLASSES:
        sys.exit(
            f"Model class names look wrong: {model.names}. "
            f"Expected {sorted(EXPECTED_CLASSES, key=int)}."
        )

    grand_total = 0
    grand_bills = 0
    for img in images:
        result = predict_one(model, img, args.conf, args.iou, args.device, out_dir)
        grand_total += result["total"]
        grand_bills += len(result["bills"])

    if len(images) > 1:
        print("\n" + "=" * 40)
        print(f"  Across {len(images)} images: {grand_bills} bills, P{grand_total} total")
        print("=" * 40)

    print(f"\nAnnotated images saved to: {out_dir}")


if __name__ == "__main__":
    main()
