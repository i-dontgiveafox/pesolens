#!/usr/bin/env python3
"""Run saved camera captures through the deployed PesoLens TFLite model."""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
from PIL import Image

try:
    from pillow_heif import register_heif_opener

    register_heif_opener()
except ImportError:
    pass

try:
    from ai_edge_litert.interpreter import Interpreter
except ImportError as exc:
    raise SystemExit(
        "ai-edge-litert is required: python -m pip install ai-edge-litert"
    ) from exc


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MODEL = (
    ROOT
    / "model_exports"
    / "kaggle-yolov8n-imgsz224-merged-6class"
    / "best_full_integer_quant.tflite"
)
DEFAULT_CLASS_NAMES = ("20", "50", "100", "200", "500", "1000")


def letterbox(image: Image.Image, size: int, pad_value: int = 114) -> np.ndarray:
    width, height = image.size
    scale = min(size / width, size / height)
    resized_width = round(width * scale)
    resized_height = round(height * scale)
    resized = image.resize(
        (resized_width, resized_height), Image.Resampling.BILINEAR
    )
    canvas = Image.new("RGB", (size, size), (pad_value, pad_value, pad_value))
    canvas.paste(
        resized,
        ((size - resized_width) // 2, (size - resized_height) // 2),
    )
    return np.asarray(canvas, dtype=np.uint8)


def center_crop(image: Image.Image, size: int) -> np.ndarray:
    width, height = image.size
    crop_size = min(width, height)
    left = (width - crop_size) // 2
    top = (height - crop_size) // 2
    cropped = image.crop((left, top, left + crop_size, top + crop_size))
    return np.asarray(
        cropped.resize((size, size), Image.Resampling.BILINEAR),
        dtype=np.uint8,
    )


def prepare_input(rgb: np.ndarray, input_details: dict) -> np.ndarray:
    real = rgb.astype(np.float32) / 255.0
    if input_details["dtype"] == np.float32:
        return real[None, ...]

    scale, zero_point = input_details["quantization"]
    if scale <= 0:
        raise ValueError("Quantized model input has no valid scale")
    quantized = np.rint(real / scale + zero_point)
    if input_details["dtype"] == np.int8:
        return np.clip(quantized, -128, 127).astype(np.int8)[None, ...]
    if input_details["dtype"] == np.uint8:
        return np.clip(quantized, 0, 255).astype(np.uint8)[None, ...]
    raise ValueError(f"Unsupported input dtype: {input_details['dtype']}")


def class_max_scores(
    interpreter: Interpreter,
    input_details: dict,
    output_details: dict,
    rgb: np.ndarray,
    class_names: tuple[str, ...],
) -> np.ndarray:
    interpreter.set_tensor(
        input_details["index"],
        prepare_input(rgb, input_details),
    )
    interpreter.invoke()
    raw = interpreter.get_tensor(output_details["index"])[0]
    if output_details["dtype"] in (np.int8, np.uint8):
        output_scale, output_zero = output_details["quantization"]
        output = (raw.astype(np.float32) - output_zero) * output_scale
    else:
        output = raw.astype(np.float32)
    if output.shape[0] != 4 + len(class_names):
        output = output.T
    if output.shape[0] != 4 + len(class_names):
        raise ValueError(
            f"Output shape {tuple(raw.shape)} does not match "
            f"{len(class_names)} model classes"
        )
    return output[4:].max(axis=1)


def parse_class_names(value: str) -> tuple[str, ...]:
    names = tuple(item.strip() for item in value.split(",") if item.strip())
    if not names:
        raise argparse.ArgumentTypeError("Class list cannot be empty")
    return names


def find_image(images_dir: Path, stem: str) -> Path | None:
    matches = [path for path in images_dir.glob(f"{stem}.*") if path.is_file()]
    return matches[0] if matches else None


def evaluate_dataset(
    dataset: Path,
    source_classes: tuple[str, ...],
    model_classes: tuple[str, ...],
    interpreter: Interpreter,
    input_details: dict,
    output_details: dict,
    threshold: float,
) -> int:
    images_dir = dataset / "images"
    labels_dir = dataset / "labels"
    if not images_dir.is_dir() or not labels_dir.is_dir():
        raise SystemExit(
            f"Dataset split must contain images/ and labels/: {dataset}"
        )

    counts: Counter[str] = Counter()
    correct: Counter[str] = Counter()
    predictions: dict[str, Counter[str]] = defaultdict(Counter)
    skipped: Counter[str] = Counter()

    input_size = int(input_details["shape"][1])
    for label_path in sorted(labels_dir.glob("*.txt")):
        class_ids = {
            int(line.split()[0])
            for line in label_path.read_text(
                encoding="utf-8", errors="replace"
            ).splitlines()
            if line.strip()
        }
        if len(class_ids) != 1:
            skipped["empty_or_multi_class"] += 1
            continue
        source_id = next(iter(class_ids))
        if source_id >= len(source_classes):
            skipped["unknown_class_id"] += 1
            continue
        image_path = find_image(images_dir, label_path.stem)
        if image_path is None:
            skipped["missing_image"] += 1
            continue

        image = Image.open(image_path).convert("RGB")
        scores = class_max_scores(
            interpreter,
            input_details,
            output_details,
            center_crop(image, input_size),
            model_classes,
        )
        best_class = int(np.argmax(scores))
        predicted = (
            model_classes[best_class]
            if scores[best_class] >= threshold
            else "none"
        )
        expected = source_classes[source_id]
        counts[expected] += 1
        correct[expected] += predicted == expected
        predictions[expected][predicted] += 1

    print(f"\nDataset: {dataset}")
    print(f"Model classes: {', '.join(model_classes)}")
    print(f"Threshold: {threshold:.2f}")
    total = sum(counts.values())
    total_correct = sum(correct.values())
    for name in source_classes:
        sample_count = counts[name]
        if sample_count == 0:
            print(f"  {name}: no single-class samples")
            continue
        accuracy = correct[name] / sample_count
        print(
            f"  {name}: {correct[name]}/{sample_count} "
            f"({accuracy:.1%}); predictions={dict(predictions[name])}"
        )
    print(f"Overall: {total_correct}/{total} ({total_correct / total:.1%})")
    if skipped:
        print(f"Skipped: {dict(skipped)}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", type=Path, nargs="*")
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    parser.add_argument("--threshold", type=float, default=0.25)
    parser.add_argument(
        "--model-classes",
        type=parse_class_names,
        default=DEFAULT_CLASS_NAMES,
        help="Comma-separated model output order.",
    )
    parser.add_argument(
        "--dataset",
        type=Path,
        help="YOLO split directory containing images/ and labels/.",
    )
    parser.add_argument(
        "--source-classes",
        type=parse_class_names,
        help="Comma-separated class order used by --dataset labels.",
    )
    parser.add_argument(
        "--compare-letterbox",
        action="store_true",
        help="Also run Ultralytics-style letterboxing for comparison.",
    )
    args = parser.parse_args()

    interpreter = Interpreter(model_path=str(args.model))
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]

    shape = tuple(int(value) for value in input_details["shape"])
    if len(shape) != 4 or shape[0] != 1 or shape[3] != 3:
        raise SystemExit(f"Unsupported model input contract: {shape}")

    if args.dataset is not None:
        if args.source_classes is None:
            raise SystemExit("--source-classes is required with --dataset")
        evaluate_dataset(
            args.dataset,
            args.source_classes,
            args.model_classes,
            interpreter,
            input_details,
            output_details,
            args.threshold,
        )
    if not args.images and args.dataset is None:
        parser.error("provide image paths or --dataset")

    modes = [("firmware-crop", center_crop)]
    if args.compare_letterbox:
        modes.append(("letterbox", letterbox))

    for path in args.images:
        image = Image.open(path).convert("RGB")
        print(f"\n{path}")
        for mode_name, preprocess in modes:
            scores = class_max_scores(
                interpreter,
                input_details,
                output_details,
                preprocess(image, shape[1]),
                args.model_classes,
            )
            best_class = int(np.argmax(scores))
            accepted = scores[best_class] >= args.threshold
            score_text = " ".join(
                f"{name}={scores[index]:.3f}"
                for index, name in enumerate(args.model_classes)
            )
            result = (
                args.model_classes[best_class]
                if accepted
                else "no detection"
            )
            print(f"  {mode_name}: result={result}; {score_text}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
