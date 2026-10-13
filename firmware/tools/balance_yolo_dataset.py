#!/usr/bin/env python3
"""Count and balance a YOLO-format dataset without editing the source dataset."""

from __future__ import annotations

import argparse
import random
import shutil
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

import yaml


IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


@dataclass(frozen=True)
class LabeledImage:
    image: Path
    label: Path
    classes: tuple[int, ...]


def read_yaml(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return yaml.safe_load(handle)


def write_yaml(path: Path, data: dict) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        yaml.safe_dump(data, handle, sort_keys=False)


def parse_label(path: Path) -> tuple[int, ...]:
    classes: list[int] = []
    if not path.is_file():
        return ()
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.strip().split()
        if parts:
            classes.append(int(float(parts[0])))
    return tuple(classes)


def find_image(images_dir: Path, stem: str) -> Path | None:
    for suffix in IMAGE_SUFFIXES:
        candidate = images_dir / f"{stem}{suffix}"
        if candidate.is_file():
            return candidate
    return None


def load_split(dataset: Path, split: str) -> list[LabeledImage]:
    labels_dir = dataset / split / "labels"
    images_dir = dataset / split / "images"
    rows: list[LabeledImage] = []
    if not labels_dir.is_dir():
        return rows

    for label in sorted(labels_dir.glob("*.txt")):
        image = find_image(images_dir, label.stem)
        if image is None:
            continue
        classes = parse_label(label)
        if classes:
            rows.append(LabeledImage(image=image, label=label, classes=classes))
    return rows


def object_counts(rows: list[LabeledImage]) -> Counter[int]:
    counts: Counter[int] = Counter()
    for row in rows:
        counts.update(row.classes)
    return counts


def primary_class(row: LabeledImage) -> int:
    # Most PesoLens images are one-bill images. For multi-object images, use the
    # most common class in that label file as the balancing bucket.
    return Counter(row.classes).most_common(1)[0][0]


def print_counts(dataset: Path, names: list[str]) -> None:
    print(f"dataset: {dataset}")
    for split in ("train", "valid", "test"):
        rows = load_split(dataset, split)
        counts = object_counts(rows)
        print(f"\n{split}: {len(rows)} labeled images")
        for idx, name in enumerate(names):
            print(f"  {idx} {name}: {counts[idx]} objects")


def copy_row(row: LabeledImage, out_images: Path, out_labels: Path, suffix: str = "") -> None:
    image_dest = out_images / f"{row.image.stem}{suffix}{row.image.suffix.lower()}"
    label_dest = out_labels / f"{row.label.stem}{suffix}.txt"
    shutil.copy2(row.image, image_dest)
    shutil.copy2(row.label, label_dest)


def copy_split_original(dataset: Path, out_dataset: Path, split: str) -> None:
    rows = load_split(dataset, split)
    out_images = out_dataset / split / "images"
    out_labels = out_dataset / split / "labels"
    out_images.mkdir(parents=True, exist_ok=True)
    out_labels.mkdir(parents=True, exist_ok=True)
    for row in rows:
        copy_row(row, out_images, out_labels)


def balance_train_split(dataset: Path, out_dataset: Path, names: list[str], seed: int) -> None:
    rng = random.Random(seed)
    rows = load_split(dataset, "train")
    if not rows:
        raise RuntimeError("No labeled training images found")

    out_images = out_dataset / "train" / "images"
    out_labels = out_dataset / "train" / "labels"
    out_images.mkdir(parents=True, exist_ok=True)
    out_labels.mkdir(parents=True, exist_ok=True)

    by_primary_class: dict[int, list[LabeledImage]] = defaultdict(list)
    by_member_class: dict[int, list[LabeledImage]] = defaultdict(list)
    for row in rows:
        by_primary_class[primary_class(row)].append(row)
        for class_id in set(row.classes):
            by_member_class[class_id].append(row)

    image_targets = {idx: len(by_primary_class[idx]) for idx in range(len(names))}
    counts = object_counts(rows)
    target = max(counts.values())

    print("\ntrain image buckets before balancing:")
    for idx, name in enumerate(names):
        print(f"  {idx} {name}: {image_targets[idx]} images, {counts[idx]} objects")
    print(f"oversampling object counts toward {target} objects")

    for row in rows:
        copy_row(row, out_images, out_labels)

    for idx in range(len(names)):
        bucket = list(by_member_class[idx])
        if not bucket:
            print(f"WARNING: class {idx} {names[idx]} has no training images")
            continue

        duplicate_index = 0
        while counts[idx] < target:
            row = rng.choice(bucket)
            copy_row(row, out_images, out_labels, suffix=f"__bal_{idx}_{duplicate_index:04d}")
            counts.update(row.classes)
            duplicate_index += 1

    print("\ntrain object counts after balancing:")
    for idx, name in enumerate(names):
        print(f"  {idx} {name}: {counts[idx]} objects")


def write_balanced_yaml(source_yaml: Path, out_dataset: Path) -> Path:
    data = read_yaml(source_yaml)
    data["path"] = str(out_dataset.resolve())
    data["train"] = "train/images"
    data["val"] = "valid/images"
    data["test"] = "test/images"
    out_yaml = out_dataset / "data.yaml"
    write_yaml(out_yaml, data)
    return out_yaml


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True, help="Source data.yaml")
    parser.add_argument("--out", type=Path, help="Balanced output dataset directory")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--count-only", action="store_true")
    args = parser.parse_args()

    source_yaml = args.data.resolve()
    data = read_yaml(source_yaml)
    source_dataset = source_yaml.parent
    names = list(data["names"])

    print_counts(source_dataset, names)
    if args.count_only:
        return 0

    if args.out is None:
        raise SystemExit("--out is required unless --count-only is used")

    out_dataset = args.out.resolve()
    if out_dataset.exists():
        raise FileExistsError(f"Output already exists: {out_dataset}")

    balance_train_split(source_dataset, out_dataset, names, args.seed)
    copy_split_original(source_dataset, out_dataset, "valid")
    copy_split_original(source_dataset, out_dataset, "test")
    out_yaml = write_balanced_yaml(source_yaml, out_dataset)

    print_counts(out_dataset, names)
    print(f"\nbalanced data.yaml: {out_yaml}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
