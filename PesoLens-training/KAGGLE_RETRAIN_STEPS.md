# PesoLens Kaggle Retraining Steps

Last updated: 2026-06-09

> Current six-class model status: **not release-ready**. Full source replay
> found severe failures for P20, P500, and P1000. Retrain and pass the
> acceptance checks below before embedding another model.

Use this document for the current merged 6-class workflow:

```text
20, 50, 100, 200, 500, 1000
```

The current firmware checkpoint uses `imgsz=224` and has been uploaded in the D6 manual-capture firmware. If the ESP32-S3 runtime remains too slow, the preferred later path is a tiny 7-class INT8 classifier. Repeating this YOLO workflow at `imgsz=160` is a fallback experiment, not the main speed plan.

## Current Kaggle Inputs

The latest Kaggle notebook used a private Kaggle dataset mounted under:

```text
/kaggle/input/datasets/jamesasperilla/pesolens-may-30
```

The previous Kaggle input contained:

```text
New Bills.yolov8 : 345 images
Old Bills.yolov8 : 330 images
```

That New Bills count was incomplete. The local source ZIP has 667 images:
345 JPG and 322 HEIC. The old merge cell accepted only JPG/JPEG/PNG, silently
dropping every HEIC image. This particularly reduced the new-bill training
data to only 7 P1000 JPGs and 14 P500 JPGs.

The complete source should produce:

```text
997 images total
995 single-class labeled images
2 empty/background images

P20:   60
P50:   331
P100:  274
P200:  60
P500:  150
P1000: 120
```

Do not start training unless the merged-data audit matches these counts.

The source `data.yaml` files had different class naming:

```text
New: 100 pesos, 1000 pesos, 50 pesos, 500 pesos
Old: 100 peso, 1000 peso, 20 peso, 200 peso, 50 peso, 500 peso
```

Merge/remap them into this canonical order:

```python
class_names = ['20 peso', '50 peso', '100 peso', '200 peso', '500 peso', '1000 peso']
```

## Merge Cell

Install the required readers first:

```python
!pip install -q ultralytics pillow-heif
```

Then create the merged dataset:

```python
from pathlib import Path
import shutil
import yaml
from collections import Counter
from PIL import Image
from pillow_heif import register_heif_opener

register_heif_opener()

base = Path("/kaggle/input/datasets/jamesasperilla/pesolens-may-30")
out = Path("/kaggle/working/pesolens_merged")
if out.exists():
    shutil.rmtree(out)

class_names = ['20 peso', '50 peso', '100 peso', '200 peso', '500 peso', '1000 peso']

datasets = {
    "new": {
        "path": base / "New Bills.yolov8",
        "names": ['100 pesos', '1000 pesos', '50 pesos', '500 pesos'],
    },
    "old": {
        "path": base / "Old Bills.yolov8",
        "names": ['100 peso', '1000 peso', '20 peso', '200 peso', '50 peso', '500 peso'],
    },
}

def normalize_name(name):
    return name.replace("pesos", "peso").strip()

merged_index = {name: i for i, name in enumerate(class_names)}

for split in ["train", "valid", "test"]:
    (out / split / "images").mkdir(parents=True, exist_ok=True)
    (out / split / "labels").mkdir(parents=True, exist_ok=True)

image_suffixes = {".jpg", ".jpeg", ".png", ".heic", ".heif"}

for prefix, info in datasets.items():
    source_to_merged = {
        i: merged_index[normalize_name(name)]
        for i, name in enumerate(info["names"])
    }

    for split in ["train", "valid", "test"]:
        img_dir = info["path"] / split / "images"
        label_dir = info["path"] / split / "labels"
        if not img_dir.exists():
            continue

        for img_path in img_dir.glob("*"):
            suffix = img_path.suffix.lower()
            if suffix not in image_suffixes:
                continue

            new_stem = f"{prefix}_{img_path.stem}"
            if suffix in {".heic", ".heif"}:
                output_image = out / split / "images" / f"{new_stem}.jpg"
                with Image.open(img_path) as image:
                    image.convert("RGB").save(output_image, quality=95)
            else:
                output_image = out / split / "images" / f"{new_stem}{suffix}"
                shutil.copy2(img_path, output_image)

            label_path = label_dir / f"{img_path.stem}.txt"
            new_label_path = out / split / "labels" / f"{new_stem}.txt"
            if label_path.exists():
                new_lines = []
                for line in label_path.read_text().splitlines():
                    parts = line.split()
                    old_class = int(parts[0])
                    parts[0] = str(source_to_merged[old_class])
                    new_lines.append(" ".join(parts))
                new_label_path.write_text("\n".join(new_lines) + "\n")
            else:
                new_label_path.write_text("")

data = {
    "train": str(out / "train" / "images"),
    "val": str(out / "valid" / "images"),
    "test": str(out / "test" / "images"),
    "nc": len(class_names),
    "names": class_names,
}

with open(out / "data.yaml", "w") as f:
    yaml.safe_dump(data, f, sort_keys=False)

print((out / "data.yaml").read_text())

histogram = Counter()
image_count = 0
empty_count = 0
for split in ["train", "valid", "test"]:
    image_count += sum(1 for p in (out / split / "images").glob("*") if p.is_file())
    for label_path in (out / split / "labels").glob("*.txt"):
        lines = [line for line in label_path.read_text().splitlines() if line.strip()]
        if not lines:
            empty_count += 1
            continue
        histogram.update(int(line.split()[0]) for line in lines)

expected = Counter({0: 60, 1: 331, 2: 274, 3: 60, 4: 150, 5: 120})
print("Merged images:", image_count)
print("Empty/background labels:", empty_count)
print("Class histogram:", histogram)
assert image_count == 997, image_count
assert empty_count == 2, empty_count
assert histogram == expected, (histogram, expected)
```

If `valid/images` is empty after merging, create a deterministic stratified
split by denomination and move each image together with its label. Keep the
final ESP32-camera acceptance set completely outside this merge.

## Train

```python
from ultralytics import YOLO
from pathlib import Path
from datetime import datetime

model = YOLO("yolov8n.pt")

results = model.train(
    data="/kaggle/working/pesolens_merged/data.yaml",
    epochs=50,
    imgsz=224,
    batch=8,
    project="/kaggle/working/runs",
    name=f"pesolens_merged_yolov8n_img224_{datetime.now():%Y%m%d_%H%M%S}"
)

run_dir = Path(model.trainer.save_dir)
best_pt = run_dir / "weights" / "best.pt"
assert best_pt.is_file(), best_pt
print("Training run:", run_dir)
print("Checkpoint:", best_pt)
```

Current run result:

```text
mAP50:    0.965
mAP50-95: 0.909
weakest class: 1000 peso recall around 0.673
```

## Export INT8 TFLite

```python
from ultralytics import YOLO
from pathlib import Path

data_yaml = Path("/kaggle/working/pesolens_merged/data.yaml")

assert best_pt.is_file(), "Run the training cell first; do not hard-code a run suffix."
model = YOLO(str(best_pt))

export_path = model.export(
    format="tflite",
    int8=True,
    imgsz=224,
    data=str(data_yaml)
)

print("Exported to:", export_path)
for p in Path("/kaggle/working").rglob("*.tflite"):
    print(p, p.stat().st_size, "bytes", round(p.stat().st_size / 1024 / 1024, 2), "MiB")
```

Using `model.trainer.save_dir` avoids accidentally exporting a stale
`...img224-2` checkpoint from an earlier run.

## Mandatory Model Acceptance

Before embedding an export:

1. Run it against a held-out set containing every denomination.
2. Run it against all source images as a training-pipeline sanity check.
3. Reject the model if any denomination is systematically mapped to another
   denomination, even when aggregate mAP looks high.
4. Compare float32 and full-integer TFLite results. Similar failures mean the
   problem is training/data lineage, not INT8 quantization.
5. Archive the exact merged `data.yaml`, class histogram, run directory,
   `best.pt`, TFLite metadata, and replay report together.

Local replay command after copying the export:

```powershell
python firmware\tools\diagnose_tflite_capture.py `
  --model firmware\model_exports\<run>\best_full_integer_quant.tflite `
  --dataset PesoLens-training\<held-out-split> `
  --source-classes 20,50,100,200,500,1000
```

Minimum release gate:

```text
No denomination below 90% on held-out camera images
No systematic cross-denomination mapping
Float32 and INT8 predictions agree on at least 99% of acceptance images
Firmware class order exactly: 20, 50, 100, 200, 500, 1000
```

The current firmware uses:

```text
best_full_integer_quant.tflite
3,227,889 bytes
```

## Package Outputs

```python
import shutil
from pathlib import Path

export_path = Path(export_path)
export_dir = export_path if export_path.is_dir() else export_path.parent
assert export_dir.is_dir(), export_dir

zip_base = "/kaggle/working/pesolens_yolov8n_img224_tflite_export"
shutil.make_archive(zip_base, "zip", export_dir)
print(zip_base + ".zip")
```

## Local Firmware Destination

Place/extract the export under:

```text
C:\Users\james\Projects\PesoLens-project\firmware\model_exports\kaggle-yolov8n-imgsz224-merged-6class
```

Then embed the full-integer TFLite into firmware:

```text
C:\Users\james\Projects\PesoLens-project\firmware\src\model_data.cpp
```

The current checkpoint already has this embedding done.

Current firmware note:

```text
Active env: seeed_xiao_esp32s3_tflm_button_capture
Trigger: XIAO D6 / GPIO43 to GND, active-low
Flow: press D6 -> 3000 ms aim delay -> capture -> infer -> speak amount/error -> ready beep
```
