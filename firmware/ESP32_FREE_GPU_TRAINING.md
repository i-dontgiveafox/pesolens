# ESP32-S3 PesoLens Free GPU Training Workflow

Date: 2026-05-30

This file describes the current free-GPU path used for the ESP32-S3 candidate. The detailed Kaggle cells live in:

```text
..\PesoLens-training\KAGGLE_RETRAIN_STEPS.md
```

## Current Training Result

Kaggle trained a merged 6-class YOLOv8n model at `imgsz=224`.

Classes:

```text
0 = 20 peso
1 = 50 peso
2 = 100 peso
3 = 200 peso
4 = 500 peso
5 = 1000 peso
```

Validation:

```text
all classes: P=0.990, R=0.937, mAP50=0.965, mAP50-95=0.909
weakest class: 1000 peso recall around 0.673
```

The exported firmware candidate is:

```text
model_exports\kaggle-yolov8n-imgsz224-merged-6class\best_full_integer_quant.tflite
```

Size:

```text
3,227,889 bytes, about 3.08 MiB
```

## Why Use Kaggle

The local Windows environment currently does not have a usable Python/Ultralytics export stack on PATH. Kaggle already has:

- GPU runtime
- the uploaded Old Bills and New Bills datasets
- the merged `/kaggle/working/pesolens_merged/data.yaml`
- the trained `best.pt`

So Kaggle is the preferred place to train and export TFLite.

## Current Kaggle Inputs

The notebook used a Kaggle dataset mounted at:

```text
/kaggle/input/datasets/jamesasperilla/pesolens-may-30
```

It contained:

```text
New Bills.yolov8 : 345 images
Old Bills.yolov8 : 330 images
```

## Training Cell

```python
!pip install -q ultralytics

from ultralytics import YOLO

model = YOLO("yolov8n.pt")

results = model.train(
    data="/kaggle/working/pesolens_merged/data.yaml",
    epochs=50,
    imgsz=224,
    batch=8,
    project="/kaggle/working/runs",
    name="pesolens_merged_yolov8n_img224"
)
```

## TFLite Export Cell

```python
from ultralytics import YOLO
from pathlib import Path

best_pt = Path("/kaggle/working/runs/pesolens_merged_yolov8n_img224-2/weights/best.pt")
data_yaml = Path("/kaggle/working/pesolens_merged/data.yaml")

model = YOLO(str(best_pt))

export_path = model.export(
    format="tflite",
    int8=True,
    imgsz=224,
    data=str(data_yaml)
)

print("Exported to:", export_path)
```

## Download These Files From Kaggle

Training evidence:

```text
best.pt
results.png
confusion_matrix.png
confusion_matrix_normalized.png
val_batch0_pred.jpg
```

Firmware export:

```text
best_full_integer_quant.tflite
best_int8.tflite
metadata.yaml
```

The current files were already moved into:

```text
..\PesoLens-training\runs\pesolens_merged_yolov8n_img224_kaggle
model_exports\kaggle-yolov8n-imgsz224-merged-6class
```

The current firmware has already embedded this export and uploaded the D6 manual-capture build:

```text
Active env: seeed_xiao_esp32s3_tflm_button_capture
Latest port: COM3
Trigger: XIAO D6 / GPIO43 to GND, active-low
Flow: D6 press -> 3000 ms aim delay -> capture -> infer -> speak amount/error -> ready beep
```

## Expected ESP32 Tradeoff

`imgsz=224` has similar flash size to `imgsz=160` because the stored weights are about the same. Runtime is heavier:

```text
160x160 = 25,600 pixels
224x224 = 50,176 pixels, about 1.96x heavier
```

For the ESP32 live demo, use:

- one bill at a time,
- centered bill,
- good lighting,
- close enough that the bill fills much of the camera view.

The uploaded 224 model initializes, but inference is still about 32-35 seconds on ESP32-S3. If that is too slow for the demo, the preferred later path is a tiny 7-class INT8 classifier, documented in `FUTURE_CLASSIFIER_MODEL_PLAN.md`. Retraining/exporting YOLO at `imgsz=160` remains a fallback experiment.
