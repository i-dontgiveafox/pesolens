# Roboflow To Kaggle Retraining

Use this when the dataset is already annotated in Roboflow and you want a new
PesoLens training run on Kaggle.

Current firmware-compatible target:

```text
YOLOv8n detector
imgsz=224
full-integer INT8 TFLite export
class order: 20, 50, 100, 200, 500, 1000
```

If the new dataset is one bill per image and speed matters more than bounding
boxes, the better later target is the classifier plan in
`../firmware/FUTURE_CLASSIFIER_MODEL_PLAN.md`. The steps below keep the current
firmware model contract.

## 1. Export From Roboflow

In Roboflow:

1. Open the project.
2. Create or select a dataset version.
3. Export the version as YOLO format. Prefer a YOLOv8-compatible export if it
   is available; YOLOv5 PyTorch format also works with Ultralytics because it
   has the same `images/`, `labels/`, and `data.yaml` structure.
4. Download the `.zip`.

Do not trust `data.yaml` class order blindly. This project previously had a
Roboflow class-order mismatch, so every run must audit the exported labels and
the final model names before firmware embedding.

Recommended Kaggle input path:

1. Create a private Kaggle dataset.
2. Upload the Roboflow export zip as a single file.
3. Add that Kaggle dataset to the notebook.

This avoids putting a Roboflow API key in the notebook. If you download
directly from Roboflow inside Kaggle instead, use a Kaggle secret and keep
internet enabled.

## 2. Kaggle Setup Cell

```python
!pip install -q ultralytics pillow-heif pyyaml
```

```python
from pathlib import Path
import shutil
import zipfile
import yaml
from collections import Counter
from datetime import datetime

from PIL import Image
from pillow_heif import register_heif_opener
from ultralytics import YOLO

register_heif_opener()
```

## 3. Locate And Extract The Roboflow Zip

Set `ZIP_HINT` to part of the uploaded zip filename.

```python
ZIP_HINT = "roboflow"  # change this

matches = [
    p for p in Path("/kaggle/input").rglob("*.zip")
    if ZIP_HINT.lower() in p.name.lower()
]
assert len(matches) == 1, matches

zip_path = matches[0]
raw_dir = Path("/kaggle/working/roboflow_raw")
if raw_dir.exists():
    shutil.rmtree(raw_dir)
raw_dir.mkdir(parents=True)

with zipfile.ZipFile(zip_path) as zf:
    zf.extractall(raw_dir)

print(zip_path)
print(raw_dir)
print(list(raw_dir.iterdir())[:20])
```

## 4. Normalize Into PesoLens Class Order

Edit `SOURCE_TO_CANONICAL` to match the exact names in the exported
`data.yaml`. The right side must stay in the firmware order.

```python
canonical_names = ["20", "50", "100", "200", "500", "1000"]

# Example mappings. Replace the keys with the exact names from Roboflow data.yaml.
SOURCE_TO_CANONICAL = {
    "20": "20",
    "20 peso": "20",
    "20 pesos": "20",
    "50": "50",
    "50 peso": "50",
    "50 pesos": "50",
    "100": "100",
    "100 peso": "100",
    "100 pesos": "100",
    "200": "200",
    "200 peso": "200",
    "200 pesos": "200",
    "500": "500",
    "500 peso": "500",
    "500 pesos": "500",
    "1000": "1000",
    "1000 peso": "1000",
    "1000 pesos": "1000",
}

def find_data_yaml(root: Path) -> Path:
    matches = list(root.rglob("data.yaml"))
    assert len(matches) == 1, matches
    return matches[0]

source_yaml = find_data_yaml(raw_dir)
source_data = yaml.safe_load(source_yaml.read_text())
source_names = source_data["names"]
if isinstance(source_names, dict):
    source_names = [source_names[i] for i in range(len(source_names))]

print("Roboflow names:", source_names)

canonical_index = {name: i for i, name in enumerate(canonical_names)}
source_to_target = {}
for source_idx, source_name in enumerate(source_names):
    key = str(source_name).strip()
    assert key in SOURCE_TO_CANONICAL, f"Add mapping for Roboflow class: {key}"
    target_name = SOURCE_TO_CANONICAL[key]
    source_to_target[source_idx] = canonical_index[target_name]

print("Remap:", source_to_target)
```

```python
merged = Path("/kaggle/working/pesolens_roboflow_merged")
if merged.exists():
    shutil.rmtree(merged)

for split in ["train", "valid", "test"]:
    (merged / split / "images").mkdir(parents=True, exist_ok=True)
    (merged / split / "labels").mkdir(parents=True, exist_ok=True)

source_root = source_yaml.parent
image_suffixes = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".heic", ".heif"}

for split in ["train", "valid", "test"]:
    image_dir = source_root / split / "images"
    label_dir = source_root / split / "labels"
    if not image_dir.exists():
        continue

    for image_path in image_dir.iterdir():
        if image_path.suffix.lower() not in image_suffixes:
            continue

        out_stem = image_path.stem
        if image_path.suffix.lower() in {".heic", ".heif"}:
            out_image = merged / split / "images" / f"{out_stem}.jpg"
            Image.open(image_path).convert("RGB").save(out_image, quality=95)
        else:
            out_image = merged / split / "images" / image_path.name
            shutil.copy2(image_path, out_image)

        label_path = label_dir / f"{image_path.stem}.txt"
        out_label = merged / split / "labels" / f"{out_stem}.txt"
        if not label_path.exists():
            out_label.write_text("")
            continue

        lines = []
        for raw in label_path.read_text().splitlines():
            if not raw.strip():
                continue
            parts = raw.split()
            parts[0] = str(source_to_target[int(float(parts[0]))])
            lines.append(" ".join(parts))
        out_label.write_text(("\n".join(lines) + "\n") if lines else "")

data = {
    "train": str(merged / "train" / "images"),
    "val": str(merged / "valid" / "images"),
    "test": str(merged / "test" / "images"),
    "nc": len(canonical_names),
    "names": canonical_names,
}
(merged / "data.yaml").write_text(yaml.safe_dump(data, sort_keys=False))
print((merged / "data.yaml").read_text())
```

## 5. Audit Before Training

Set `EXPECTED_COUNTS` from the Roboflow project's class counts. If you do not
know them yet, run once with `EXPECTED_COUNTS = None`, inspect the printed
histogram, then fill it in and rerun.

```python
EXPECTED_COUNTS = None
# Example:
# EXPECTED_COUNTS = {"20": 60, "50": 331, "100": 274, "200": 60, "500": 150, "1000": 120}

histogram = Counter()
empty_labels = 0
image_count = 0

for split in ["train", "valid", "test"]:
    image_count += sum(1 for p in (merged / split / "images").iterdir() if p.is_file())
    for label_path in (merged / split / "labels").glob("*.txt"):
        lines = [line for line in label_path.read_text().splitlines() if line.strip()]
        if not lines:
            empty_labels += 1
            continue
        histogram.update(int(float(line.split()[0])) for line in lines)

print("Images:", image_count)
print("Empty/background labels:", empty_labels)
print("Histogram:", {canonical_names[k]: v for k, v in sorted(histogram.items())})

if EXPECTED_COUNTS is not None:
    expected = Counter({canonical_index[name]: count for name, count in EXPECTED_COUNTS.items()})
    assert histogram == expected, (histogram, expected)
```

Also visually inspect sample labels before training:

```python
import random
from IPython.display import display

sample_images = []
for split in ["train", "valid", "test"]:
    sample_images.extend((merged / split / "images").glob("*"))

for path in random.sample(sample_images, min(12, len(sample_images))):
    display(Image.open(path).resize((320, 240)))
    print(path.name)
    print((merged / path.parents[1].name / "labels" / f"{path.stem}.txt").read_text()[:300])
```

## 6. Train

```python
model = YOLO("yolov8n.pt")

results = model.train(
    data=str(merged / "data.yaml"),
    epochs=50,
    imgsz=224,
    batch=8,
    project="/kaggle/working/runs",
    name=f"pesolens_roboflow_yolov8n_img224_{datetime.now():%Y%m%d_%H%M%S}",
)

run_dir = Path(model.trainer.save_dir)
best_pt = run_dir / "weights" / "best.pt"
assert best_pt.is_file(), best_pt
print("Run:", run_dir)
print("Best:", best_pt)
```

## 7. Verify Class Names And Export TFLite

```python
trained = YOLO(str(best_pt))
print(trained.names)
assert [str(trained.names[i]) for i in range(len(canonical_names))] == canonical_names
```

```python
export_path = trained.export(
    format="tflite",
    int8=True,
    imgsz=224,
    data=str(merged / "data.yaml"),
)

print("Exported:", export_path)
for p in Path("/kaggle/working").rglob("*.tflite"):
    print(p, p.stat().st_size, "bytes", round(p.stat().st_size / 1024 / 1024, 2), "MiB")
```

## 8. Package Outputs

```python
package_dir = Path("/kaggle/working/pesolens_package")
if package_dir.exists():
    shutil.rmtree(package_dir)
package_dir.mkdir()

shutil.copy2(best_pt, package_dir / "best.pt")
shutil.copy2(merged / "data.yaml", package_dir / "data.yaml")

export_path = Path(export_path)
export_dir = export_path if export_path.is_dir() else export_path.parent
for p in export_dir.glob("*"):
    if p.is_file():
        shutil.copy2(p, package_dir / p.name)

for name in ["results.png", "confusion_matrix.png", "confusion_matrix_normalized.png"]:
    src = run_dir / name
    if src.is_file():
        shutil.copy2(src, package_dir / name)

zip_path = shutil.make_archive("/kaggle/working/pesolens_roboflow_img224_export", "zip", package_dir)
print(zip_path)
```

Download the package zip from Kaggle, then copy the TFLite export into a new
folder under:

```text
firmware/model_exports/
```

Before embedding it, run the mandatory replay checks from
`KAGGLE_RETRAIN_STEPS.md`.

Minimum release gate remains:

```text
No denomination below 90% on held-out ESP32 camera images
No systematic cross-denomination mapping
Float32 and INT8 predictions agree on at least 99% of acceptance images
Firmware class order exactly: 20, 50, 100, 200, 500, 1000
```
