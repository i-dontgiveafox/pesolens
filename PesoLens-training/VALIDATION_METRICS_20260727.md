# PesoLens — Mean Average Precision of Each Class

Generated: 2026-07-27 (locally, on this PC)

## Which model this is

- Model: **`pesolens_best.pt`** — the deployed 2026-07-07 "thesis2" YOLOv8n model
  (the one embedded in the ESP32-S3 firmware).
- Classes: **5** — `20, 50, 100, 500, 1000`. **There is no ₱200 in this model.**
  (If you have seen a 6-class table with a ₱200 row, that is a *different*, older
  model — not this one.)
- Validation set: the `valid` split of the merged Thesis 2 dataset (567 images,
  664 bill instances), rebuilt locally with the exact same 9→5 class remap used
  for training.
- Command: `YOLO("pesolens_best.pt").val(data=<merged>/data.yaml, split="val", imgsz=320)`
  (ultralytics 8.4.76, torch CPU).

## Trust check (why these numbers are valid)

The aggregate row reproduces the training run's saved metrics almost exactly:

| Source | Precision | Recall | mAP50 | mAP50-95 |
|--------|-----------|--------|-------|----------|
| This local run          | 0.975 | 0.933 | 0.969 | 0.831 |
| `kaggle_exports/20260707/results.csv` (epoch 100) | 0.978 | 0.933 | 0.968 | 0.830 |

Matching to 3 decimals confirms the dataset/remap/class-order are correct.

## Mean Average Precision of Each Class

| Class | Images | Instances | Precision | Recall | mAP50 | mAP50-95 |
|-------|--------|-----------|-----------|--------|-------|----------|
| **all**  | 567 | 664 | 97.5% | 93.3% | 96.9% | 83.1% |
| 20 peso  | 80  | 80  | 98.5% | 81.9% | 93.2% | 77.3% |
| 50 peso  | 164 | 170 | 96.4% | 93.9% | 97.8% | 83.1% |
| 100 peso | 140 | 140 | 93.6% | 94.3% | 94.7% | 78.6% |
| 500 peso | 128 | 128 | 100%  | 100%  | 99.5% | 90.5% |
| 1000 peso| 143 | 146 | 98.8% | 96.6% | 99.3% | 86.1% |

Raw values (for reference): all P0.975 R0.933 mAP50 0.969 mAP50-95 0.831;
20 P0.985 R0.819 mAP50 0.932 mAP50-95 0.773; 50 P0.964 R0.939 mAP50 0.978
mAP50-95 0.831; 100 P0.936 R0.943 mAP50 0.947 mAP50-95 0.786; 500 P1.000 R1.000
mAP50 0.995 mAP50-95 0.905; 1000 P0.988 R0.966 mAP50 0.993 mAP50-95 0.861.

Weakest class is **P20** (recall 81.9%) — prioritize collecting more ₱20 data if
retraining. P500 is now essentially perfect (was the old weak class at 0.41
recall in the 2026-06-25 model).

## Supporting plots

- Fresh plots from this run: `scratchpad/val_runs/pesolens_val/`
  (`confusion_matrix.png`, `BoxPR_curve.png`, etc.) — copy them into the project
  if you want to keep them.
- Original training-run plots (same model): `kaggle_exports/20260707/`
  (`confusion_matrix.png`, `confusion_matrix_normalized.png`, `BoxPR_curve.png`,
  `BoxP_curve.png`, `BoxR_curve.png`, `BoxF1_curve.png`, `results.png`).

## How to reproduce (scripts saved in the project)

```powershell
cd "C:\Users\james\Projects\PesoLens-project\PesoLens-training"
.\.venv\Scripts\python.exe -m pip install pillow-heif   # once, for HEIC decode
.\.venv\Scripts\python.exe build_local_val_set.py        # builds local_val_dataset/
.\.venv\Scripts\python.exe run_local_val.py              # prints the per-class table
```

- `build_local_val_set.py` reproduces the Kaggle merge for the valid split only:
  9-class Roboflow exports → remap `{0:2,1:4,2:1,3:3,4:2,5:4,6:0,7:1,8:3}` →
  target order `[20,50,100,500,1000]`, converting HEIC→JPG.
- Source exports (must exist): `Downloads/Thesis 2.yolov8 (1)` and
  `Downloads/Thesis 2 additional set.yolov8`.
- For the held-out **test** split (566 images) instead, change `SPLIT = 'valid'`
  to `SPLIT = 'test'` in `build_local_val_set.py`, rerun both scripts.
- Output dataset lands in `PesoLens-training/local_val_dataset/`; val plots
  (confusion matrix, PR curves) in `PesoLens-training/local_val_runs/`.
