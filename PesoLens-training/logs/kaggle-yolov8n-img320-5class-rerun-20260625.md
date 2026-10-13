# Kaggle YOLOv8n 5-Class Rerun Checkpoint

Date recorded: 2026-06-25

## Context

The earlier Kaggle output was lost because the Kaggle session reset and
`/kaggle/working` only contained `.virtual_documents`. The model, Python
variables, merged dataset, and YOLO run folder were gone, so training had to be
rerun.

## Successful Rerun

User reported the rerun completed and saved results here:

```text
/kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_154332
```

Best checkpoint:

```text
/kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_154332/weights/best.pt
```

Reported save confirmation:

```text
Results saved to /kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_154332
Run: /kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_154332
Best exists: True /kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_154332/weights/best.pt
Download this now:
/kaggle/working/pesolens_training_output.zip
```

The training again stopped at epoch 81. This is acceptable because the run used
EarlyStopping with `patience=20`; it was configured for up to 100 epochs but
stopped after the model stopped improving.

## Class Order Verification

User verified the trained model names:

```python
{0: '20', 1: '50', 2: '100', 3: '500', 4: '1000'}
```

This is the correct five-class order for this run:

```text
20, 50, 100, 500, 1000
```

Firmware must be updated to five classes before embedding any exported model
from this run. Do not use this model with firmware that still expects a
six-class output including `200`.

## Files To Preserve In Kaggle

Before leaving the Kaggle session idle, preserve these outputs:

```text
/kaggle/working/pesolens_training_output.zip
/kaggle/working/pesolens_full_training_results.zip
/kaggle/working/pesolens_tflite_export.zip
```

At minimum, keep `pesolens_training_output.zip` because it contains the YOLO
run folder and `weights/best.pt`.

The YOLO run folder should contain useful training artifacts such as:

```text
weights/best.pt
weights/last.pt
results.png
results.csv
confusion_matrix.png
confusion_matrix_normalized.png
PR_curve.png
F1_curve.png
P_curve.png
R_curve.png
```

## Kaggle Save Checklist

Recommended before sleeping or closing the browser:

1. Download `pesolens_training_output.zip`.
2. Download `pesolens_full_training_results.zip` if created.
3. Download `pesolens_tflite_export.zip` if TFLite export was created.
4. In Kaggle, click **Save Version**.
5. Use a saved version option that preserves notebook output.
6. Confirm the Kaggle version finishes saving before closing the tab.

If there is time, create a persistent Kaggle dataset from the important output
zips. A Kaggle dataset is safer than relying only on the live
`/kaggle/working` session.

## Next Actions

1. Confirm all zip files were downloaded locally.
2. If TFLite export has not been created yet, export from `best.pt`.
3. Copy the downloaded artifacts into the local project.
4. Run held-out ESP32 camera-image tests, especially for 500-peso bills.
5. Update firmware class count/order before embedding the five-class model.
