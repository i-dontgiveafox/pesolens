# PesoLens Kaggle / Roboflow Checkpoint

> **Superseded in places.** The 2026-09-28 section at the top of `CURRENT_CHECKPOINT.md` is the current state and wins wherever this file disagrees (e.g. latency is measured at 72.9 s, audio is confirmed, the 320px/5-class model is flashed, no P200).

Date: 2026-06-24

## Current Goal

Retrain the PesoLens model in Kaggle from a new Roboflow YOLOv8 export.

Firmware-compatible target remains:

```text
YOLOv8n
imgsz=224
full-integer INT8 TFLite export
class order: 20, 50, 100, 200, 500, 1000
```

## Local Roboflow Export

The latest local cleaned export file was:

```text
C:\Users\james\Downloads\thesis2_yolov8_clean\new-pesolens.zip
```

This was uploaded to Kaggle as a dataset named:

```text
new-pesolens
```

Kaggle extracted it automatically, so the notebook did not find a `.zip` under
`/kaggle/input`.

Detected Kaggle dataset path:

```text
/kaggle/input/datasets/jamesasperilla/new-pesolens
```

Detected Kaggle data YAML:

```text
/kaggle/input/datasets/jamesasperilla/new-pesolens/data.yaml
```

## Notebook State

The Kaggle environment is using a GPU accelerator. `torch.cuda.is_available()`
printed:

```text
True
```

The pip install cell completed with dependency warnings about Kaggle's
preinstalled RAPIDS/Dask packages. Those warnings are acceptable because this
workflow uses Ultralytics/PyTorch, not RAPIDS.

## Class Remap Output

The class-order audit cell printed:

```text
Roboflow class names: ['new_100', 'new_1000', 'new_50', 'new_500', 'old_100', 'old_1000', 'old_20', 'old_50', 'old_500']
Class remap: {0: 2, 1: 5, 2: 1, 3: 4, 4: 2, 5: 5, 6: 0, 7: 1, 8: 4}
```

This remap is correct for the exported classes:

```text
new_100   -> 100
new_1000  -> 1000
new_50    -> 50
new_500   -> 500
old_100   -> 100
old_1000  -> 1000
old_20    -> 20
old_50    -> 50
old_500   -> 500
```

## Scope Update

The team confirmed there are no 200-peso bills available and the project can
leave 200-peso recognition out of this training run.

Do not train a six-class model with an empty 200-peso class. Instead, switch
this run to a clean five-class target:

```text
20, 50, 100, 500, 1000
```

The current firmware still expects the old six-class order:

```text
0 -> 20
1 -> 50
2 -> 100
3 -> 200
4 -> 500
5 -> 1000
```

So after training the five-class model, update firmware before embedding:

```text
MODEL_NUM_CLASSES = 5
MODEL_CLASSES = 20, 50, 100, 500, 1000
```

Do not flash a five-class TFLite model into firmware that still expects six
classes. The output channel count will be different.

## Next Steps

Latest reported Kaggle run log:

```text
PesoLens-training/logs/kaggle-yolov8n-img320-5class-rerun-20260625.md
```

This `imgsz=320`, five-class rerun was configured for 100 epochs and completed
normally at epoch 81 due to EarlyStopping. The user verified `best.pt` exists
and the trained class order is `{0: '20', 1: '50', 2: '100', 3: '500',
4: '1000'}`. The immediate priority is preserving Kaggle outputs before the
session resets again.

1. Continue in the Kaggle notebook using five canonical classes:

```python
canonical_names = ["20", "50", "100", "500", "1000"]
```

2. Rerun the class-order/remap audit.
3. Build the merged dataset.
4. Audit counts for the five classes.
5. Train YOLOv8n at `imgsz=224`.
6. Export full-integer INT8 TFLite.
7. Update firmware class count/order before embedding the new model.

After the class list is fixed, continue with:

```text
PesoLens-training/ROBOFLOW_TO_KAGGLE_RETRAIN.md
```

and the mandatory acceptance checks in:

```text
PesoLens-training/KAGGLE_RETRAIN_STEPS.md
```
