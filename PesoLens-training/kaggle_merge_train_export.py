from __future__ import annotations

import random
import shutil
import zipfile
from collections import Counter, defaultdict
from io import BytesIO
from pathlib import Path

from PIL import Image

try:
    from pillow_heif import register_heif_opener

    register_heif_opener()
except Exception as exc:
    print(f"[warn] pillow-heif unavailable; HEIC conversion may fail: {exc}")


# Current firmware model class order. This 2026-06-25 run intentionally omits
# 200 because the latest Kaggle export was trained as a five-class model.
TARGET_DENOMS = [20, 50, 100, 500, 1000]

IMG_SIZE = 320
EPOCHS = 100
BATCH = 8
VAL_FRACTION = 0.20
SEED = 0

OUT_DIR = Path("/kaggle/working/pesolens_roboflow_merged")
RUNS_DIR = Path("/kaggle/working/runs")

OLD_CLASS_TO_DENOM = {
    0: 100,
    1: 1000,
    2: 20,
    3: 200,
    4: 50,
    5: 500,
}

NEW_CLASS_TO_DENOM = {
    0: 100,
    1: 1000,
    2: 50,
    3: 500,
}


def find_zip(required_words: list[str]) -> Path:
    root = Path("/kaggle/input")
    matches = []
    for path in root.rglob("*.zip"):
        name = path.name.lower()
        if all(word.lower() in name for word in required_words):
            matches.append(path)
    if not matches:
        raise FileNotFoundError(f"No ZIP found in /kaggle/input matching {required_words}")
    if len(matches) > 1:
        print(f"[warn] multiple matches for {required_words}; using {matches[0]}")
    return matches[0]


def find_base_model() -> str:
    candidates = list(Path("/kaggle/input").rglob("best.pt"))
    if candidates:
        print(f"[info] using uploaded base model: {candidates[0]}")
        return str(candidates[0])
    print("[info] no uploaded best.pt found; using yolov8n.pt")
    return "yolov8n.pt"


def label_name_for_image(image_name: str) -> str:
    path = Path(image_name)
    return f"train/labels/{path.stem}.txt"


def parse_labels(text: str, class_to_denom: dict[int, int]) -> tuple[list[str], int | None, bool]:
    target_index = {denom: idx for idx, denom in enumerate(TARGET_DENOMS)}
    remapped: list[str] = []
    primary_class: int | None = None
    has_excluded_class = False

    for raw in text.splitlines():
        raw = raw.strip()
        if not raw:
            continue
        parts = raw.split()
        src_cls = int(parts[0])
        denom = class_to_denom[src_cls]
        if denom not in target_index:
            has_excluded_class = True
            continue
        dst_cls = target_index[denom]
        if primary_class is None:
            primary_class = dst_cls
        remapped.append(" ".join([str(dst_cls), *parts[1:]]))

    return remapped, primary_class, has_excluded_class


def save_image(zf: zipfile.ZipFile, entry: zipfile.ZipInfo, out_path: Path) -> None:
    suffix = Path(entry.filename).suffix.lower()
    data = zf.read(entry)
    if suffix == ".heic":
        image = Image.open(BytesIO(data)).convert("RGB")
        image.save(out_path, format="JPEG", quality=92)
    else:
        out_path.write_bytes(data)


def import_dataset(zip_path: Path, prefix: str, class_to_denom: dict[int, int], staging: Path) -> list[dict]:
    image_dir = staging / "images"
    label_dir = staging / "labels"
    image_dir.mkdir(parents=True, exist_ok=True)
    label_dir.mkdir(parents=True, exist_ok=True)

    samples = []
    skipped_missing_label = 0
    skipped_empty = 0
    skipped_excluded = 0
    object_counts = Counter()

    with zipfile.ZipFile(zip_path, "r") as zf:
        labels = {entry.filename: entry for entry in zf.infolist() if entry.filename.startswith("train/labels/")}
        images = [
            entry
            for entry in zf.infolist()
            if entry.filename.startswith("train/images/")
            and Path(entry.filename).suffix.lower() in {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".heic"}
        ]

        for img_entry in images:
            label_entry = labels.get(label_name_for_image(img_entry.filename))
            if label_entry is None:
                skipped_missing_label += 1
                continue

            label_text = zf.read(label_entry).decode("utf-8", errors="replace")
            remapped, primary_class, has_excluded_class = parse_labels(label_text, class_to_denom)

            # For the 4-class firmware model, drop old-bill 20/200 images instead
            # of training with unlabelled non-target bills visible in the frame.
            if has_excluded_class:
                skipped_excluded += 1
                continue
            if not remapped or primary_class is None:
                skipped_empty += 1
                continue

            safe_stem = f"{prefix}_{Path(img_entry.filename).stem}"
            out_img = image_dir / f"{safe_stem}.jpg"
            out_label = label_dir / f"{safe_stem}.txt"

            save_image(zf, img_entry, out_img)
            out_label.write_text("\n".join(remapped) + "\n", encoding="utf-8")

            for line in remapped:
                object_counts[int(line.split()[0])] += 1
            samples.append({"image": out_img, "label": out_label, "primary_class": primary_class})

    print(f"\n[{prefix}] imported {len(samples)} images from {zip_path.name}")
    print(f"[{prefix}] skipped missing-label={skipped_missing_label}, empty={skipped_empty}, excluded-class={skipped_excluded}")
    print(f"[{prefix}] object counts: {dict(sorted(object_counts.items()))}")
    return samples


def split_samples(samples: list[dict]) -> None:
    random.seed(SEED)
    for split in ["train", "valid"]:
        (OUT_DIR / split / "images").mkdir(parents=True, exist_ok=True)
        (OUT_DIR / split / "labels").mkdir(parents=True, exist_ok=True)

    by_class = defaultdict(list)
    for sample in samples:
        by_class[sample["primary_class"]].append(sample)

    split_counts = {"train": Counter(), "valid": Counter()}
    for cls, items in by_class.items():
        random.shuffle(items)
        n_valid = max(1, round(len(items) * VAL_FRACTION))
        valid_items = items[:n_valid]
        train_items = items[n_valid:]

        for split, split_items in [("train", train_items), ("valid", valid_items)]:
            for sample in split_items:
                dst_img = OUT_DIR / split / "images" / sample["image"].name
                dst_label = OUT_DIR / split / "labels" / sample["label"].name
                shutil.move(str(sample["image"]), dst_img)
                shutil.move(str(sample["label"]), dst_label)
                for line in dst_label.read_text(encoding="utf-8").splitlines():
                    if line.strip():
                        split_counts[split][int(line.split()[0])] += 1

    print("\n[split] object counts by split")
    for split, counts in split_counts.items():
        print(f"{split}: {dict(sorted(counts.items()))}")


def write_data_yaml() -> Path:
    names = ", ".join(f"'{denom}'" for denom in TARGET_DENOMS)
    data_yaml = OUT_DIR / "data.yaml"
    data_yaml.write_text(
        "\n".join(
            [
                f"path: {OUT_DIR}",
                "train: train/images",
                "val: valid/images",
                f"nc: {len(TARGET_DENOMS)}",
                f"names: [{names}]",
                "",
            ]
        ),
        encoding="utf-8",
    )
    print("\n[data.yaml]")
    print(data_yaml.read_text(encoding="utf-8"))
    return data_yaml


def train_validate_export(data_yaml: Path) -> None:
    from ultralytics import YOLO

    base_model = find_base_model()
    model = YOLO(base_model)
    results = model.train(
        data=str(data_yaml),
        imgsz=IMG_SIZE,
        epochs=EPOCHS,
        batch=BATCH,
        patience=20,
        device=0,
        workers=2,
        project=str(RUNS_DIR),
        name=f"pesolens_{len(TARGET_DENOMS)}class_yolov8n_img{IMG_SIZE}_e{EPOCHS}",
        fliplr=0.0,
        flipud=0.0,
        mosaic=0.1,
        close_mosaic=10,
        degrees=15.0,
        translate=0.05,
        scale=0.15,
        perspective=0.0005,
        hsv_h=0.01,
        hsv_s=0.2,
        hsv_v=0.2,
    )

    best = Path(results.save_dir) / "weights" / "best.pt"
    print(f"\n[best.pt] {best}")

    model = YOLO(str(best))
    model.val(data=str(data_yaml), imgsz=IMG_SIZE, device=0)

    exported = model.export(format="tflite", int8=True, imgsz=IMG_SIZE, data=str(data_yaml))
    print(f"\n[exported] {exported}")

    print("\n[TFLite files]")
    for path in Path("/kaggle/working").rglob("*.tflite"):
        print(f"{path.stat().st_size / (1024 * 1024):.2f} MB | {path}")


def main() -> None:
    if OUT_DIR.exists():
        shutil.rmtree(OUT_DIR)
    staging = Path("/kaggle/working/pesolens_staging")
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    old_zip = find_zip(["old", "bills"])
    new_zip = find_zip(["new", "bills"])
    print(f"[zip] old={old_zip}")
    print(f"[zip] new={new_zip}")
    print(f"[classes] {TARGET_DENOMS}")

    samples = []
    samples += import_dataset(old_zip, "old", OLD_CLASS_TO_DENOM, staging)
    samples += import_dataset(new_zip, "new", NEW_CLASS_TO_DENOM, staging)
    split_samples(samples)
    data_yaml = write_data_yaml()
    train_validate_export(data_yaml)


if __name__ == "__main__":
    main()
