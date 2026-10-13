# PesoLens Handover

> **Superseded in places.** The 2026-09-28 section at the top of `CURRENT_CHECKPOINT.md` is the current state and wins wherever this file disagrees (e.g. latency is measured at 72.9 s, audio is confirmed, the 320px/5-class model is flashed, no P200).

Last updated: 2026-07-07

## 2026-07-07 Current Checkpoint (Thesis 2 retrain)

Retrained on two Roboflow exports (`Thesis 2.yolov8` + `Thesis 2 additional
set.yolov8`), merged and remapped from 9 new/old classes down to the 5
firmware denominations. Kaggle notebook: `KAGGLE_RETRAIN_THESIS2.md`.

Active local model:

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\pesolens_best.pt
```

Class order (verified): `{0:'20', 1:'50', 2:'100', 3:'500', 4:'1000'}`

Dataset: 5024 images (3891 train / 567 valid / 566 test). Combined denomination
instance counts: 20=644, 50=1675, 100=1582, 500=1377, 1000=1374.

Training: YOLOv8n, imgsz=320, full 100 epochs (patience=100, best at epoch 98),
batch=16, full-rotation augmentation (degrees=180, flipud=0.5) for uncontrolled
bill orientation, low hue jitter (hsv_h=0.01) to preserve denomination color.

Validation results (big improvement over the 2026-06-25 model):

| Metric | 2026-06-25 | 2026-07-07 |
|--------|-----------|-----------|
| mAP50  | 0.915 | 0.969 |
| mAP50-95 | 0.769 | 0.835 |
| Recall | 0.829 | 0.933 |
| Precision | 0.836 | 0.976 |

Per-class recall: 20=0.86, 50=0.96, 100=0.95, **500=0.99** (was 0.41), 1000=0.97.
No denomination-to-denomination confusion; all errors are background miss/FP.
New weakest class is P20 (0.86) — prioritize P20 if collecting more data.

Local smoke test: `predict.py test_bill.jpg` correctly reads P100 + P50 = P150
on a messy real-world overlapping-bills photo.

Run archive (full curves, confusion matrices, sample preds, weights):

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\kaggle_exports\20260707\
```

Previous model backed up to:

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\pesolens_best_pre-thesis2_20260625.pt
```

Firmware note: this Ultralytics version exported `best_int8.tflite` (3.31 MB)
rather than the older `best_full_integer_quant.tflite` name. Size matches the
old full-integer model, but verify the TFLite input/output tensor dtype is int8
before embedding into firmware. No firmware reflash done yet for this checkpoint.

## 2026-06-25 Checkpoint

Active local model:

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\pesolens_best.pt
```

Active firmware export:

```text
C:\Users\james\Projects\PesoLens-project\firmware\model_exports\kaggle-yolov8n-imgsz320-5class-20260625\best_full_integer_quant.tflite
```

Class order:

```text
20, 50, 100, 500, 1000
```

Firmware settings:

```text
MODEL_INPUT_SIZE = 320
MODEL_NUM_CLASSES = 5
```

The Kaggle outputs downloaded on 2026-06-25 are preserved under:

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\kaggle_exports\20260625
```

Local test command:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\PesoLens-training"
.\.venv\Scripts\python.exe predict.py
```

Current result on `test_bill.jpg`: the script runs and writes an annotated
image, but the new model detects only P1000 and misses the visible P500.

This document originally described the older 4-class laptop model. Keep the older sections for historical debugging context; the 2026-06-25 checkpoint above is the current state.

## 2026-05-30 Historical Firmware Checkpoint

Current project root:

```text
C:\Users\james\Projects\PesoLens-project
```

Read first:

```text
C:\Users\james\Projects\PesoLens-project\CURRENT_CHECKPOINT.md
C:\Users\james\Projects\PesoLens-project\firmware\PROGRESS.md
```

Historical trained model:

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\runs\pesolens_merged_yolov8n_img224_kaggle\pesolens_yolov8n_img224_best.pt
```

Historical firmware TFLite export:

```text
C:\Users\james\Projects\PesoLens-project\firmware\model_exports\kaggle-yolov8n-imgsz224-merged-6class\best_full_integer_quant.tflite
```

Historical class order:

```text
20, 50, 100, 200, 500, 1000
```

Historical firmware settings:

```text
MODEL_INPUT_SIZE = 224
MODEL_NUM_CLASSES = 6
```

Status:

- Kaggle training, selected result images, and TFLite export are saved locally.
- Full-integer TFLite is embedded into `firmware\src\model_data.cpp`.
- Updated firmware has been built and flashed to the ESP32-S3 on `COM3`.
- Active firmware environment is `seeed_xiao_esp32s3_tflm_button_capture`.
- Current firmware trigger is XIAO `D6 / GPIO43` to `GND`, active-low.
- Current flow is press D6, wait 3000 ms, capture one frame, run inference, speak the amount or error tone, then play a ready beep.
- Serial logs verified camera init, `[infer] ready`, audio init, and waiting for D6/GPIO43.
- Next step is controlled real-bill testing with D6 manual capture and per-denomination serial logs.
- Runtime is still slow on ESP32-S3: about 32-35 seconds per inference. The deferred replacement-model plan is `firmware\FUTURE_CLASSIFIER_MODEL_PLAN.md`.

The older sections below still explain the previous 4-class model and the Roboflow class-name issue that caused wrong denominations.

---

## 1. What this is

PesoLens is a computer vision model that looks at a photo and identifies Philippine peso banknotes in it. It returns:
- Which denominations it sees (50, 100, 500, 1000)
- A confidence score for each
- The total peso amount

It runs locally. This checkout is currently verified with the included CPU-only `.venv`; if you install a CUDA-enabled PyTorch build later, `predict.py` can use a GPU with `--device 0`. No cloud or internet is needed at runtime.

---

## 2. Day-to-day usage

Open PowerShell, go to the project folder, and run:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\PesoLens-training"
.\.venv\Scripts\python.exe predict.py
```

That tests on the included `test_bill.jpg` and prints something like:

```
test_bill.jpg
-------------
  1 x P1000  (avg confidence 90%)
  Total: P1000
```

It also saves an annotated image (with the detection box drawn on it) into a folder called `predict_results`.

### Other ways to run it

```powershell
.\.venv\Scripts\python.exe predict.py my_photo.jpg                       # one specific image
.\.venv\Scripts\python.exe predict.py my_photos_folder                   # everything in a folder
.\.venv\Scripts\python.exe predict.py my_photo.jpg --conf 0.3            # lower threshold if it is missing bills
.\.venv\Scripts\python.exe predict.py my_photo.jpg --conf 0.7            # raise threshold if it is catching false positives
.\.venv\Scripts\python.exe predict.py my_photo.jpg --device 0            # use CUDA GPU if available
.\.venv\Scripts\python.exe predict.py my_photo.jpg --out my_results      # custom output folder
```

The default confidence threshold is 0.25 (25%). That number is the cutoff for "I'm sure enough this is a bill" — lower it to catch more (but risk false positives), raise it to be stricter. The default IoU/NMS overlap threshold is 0.3, which suppresses duplicate boxes on the included test image.

---

## 3. What lives where

```
C:\Users\james\Projects\PesoLens-project\PesoLens-training\
├── pesolens_best.pt        ← THE model. This is what predict.py uses. 50 MB.
├── predict.py              ← The inference script. Run this.
├── HANDOVER.md             ← This file.
├── best.pt                 ← The base checkpoint training started from. 50 MB.
│                              Keep it around in case you ever want to retrain
│                              from the same starting point.
├── test_bill.jpg           ← The example image used for default testing.
├── pesolens_best.onnx      ← ONNX export of the production model.
├── pesolens_best_saved_model\
│                              TensorFlow / TFLite exports.
├── pesolens-roboflow\      ← The training dataset (1801 train + 200 val images).
│   ├── data.yaml             Tells YOLO where the images are and what the
│   │                          classes mean. Has a warning comment about the
│   │                          Roboflow class-name gotcha (see Section 6).
│   ├── train\
│   ├── valid\
│   └── test\                 (currently empty — no test split was exported)
├── runs\                   ← Every training and validation run lands here.
│   └── detect\
│       ├── train-2\          The successful 30-epoch training run.
│       │   ├── weights\      best.pt and last.pt from this run.
│       │   ├── results.png   Loss curves over epochs.
│       │   └── ...           Confusion matrix, sample predictions, etc.
│       └── val_fixed\        Validation results with the corrected class names.
└── .venv\                  ← Python virtual environment. Don't touch.
```

**What you actually need:** `pesolens_best.pt` and `predict.py`. Everything else is supporting material.

---

## 4. The model's measured performance

These are the numbers from validation on 200 held-out images:

| Denomination | Validation images | Precision | Recall | mAP50-95 |
|--------------|-------------------|-----------|--------|----------|
| 50           | 71                | 99.9%     | 100%   | 98.9%    |
| 100          | 69                | 99.9%     | 100%   | 98.7%    |
| 500          | 31                | 99.7%     | 100%   | 98.6%    |
| 1000         | 29                | 99.7%     | 100%   | 97.6%    |

Translation: on the validation set, the model caught every single bill and almost never confused one denomination for another. The confusion matrix at `runs\detect\val_fixed\confusion_matrix.png` is purely diagonal — no cross-class mistakes.

**Inference speed:** the included `.venv` is CPU-only, so the first local smoke test takes a few seconds because the model has to load. The original GPU handover measured about 25 milliseconds per image on an RTX 5090.

---

## 4b. Every mistake that was fixed in this handover pass

Six concrete things were broken when we started. All six are now fixed.

| # | Mistake | Symptom | Fix |
|---|---------|---------|-----|
| 1 | `pesolens-roboflow/data.yaml` had wrong absolute paths from an older machine. | YOLO would not have been able to find the training images at all. | Rewrote the train/val/test paths as relative project paths. |
| 2 | The class names in `data.yaml` were `['50','100','500','1000']` but the actual class indices in the labels were `[100, 1000, 50, 500]` (Roboflow exports class names alphabetically as strings, so `'1000'` sorts before `'50'`). | Validation showed perfect 98.4% mAP50-95, but a real photo of a 50-peso bill got predicted as "500". | Reordered the names list to `['100','1000','50','500']` and added a warning comment explaining the Roboflow gotcha. |
| 3 | The trained model `runs/detect/train-2/weights/best.pt` had the same shuffled class names baked into its embedded `names` dict. | Even after fixing `data.yaml`, the model itself would still output wrong labels. | Wrote a small script that loads the checkpoint, rewrites the embedded `names` dict to the correct mapping, and saves the patched model as `pesolens_best.pt`. **No retraining was needed** — the underlying weights were already correct. |
| 4 | A stale `data.yaml` in the project root had wrong paths AND wrong names AND was a leftover from an older configuration. | Future training or validation runs could have accidentally loaded this file and silently used wrong settings. | Deleted it. The only remaining `data.yaml` is the correct one inside `pesolens-roboflow/`. |
| 5 | The old `test.py` had a hardcoded 6-class mapping (`{0:'20', 1:'50', 2:'100', 3:'200', 4:'500', 5:'1000'}`) from a previous training attempt that included 20 and 200 peso bills. The current model only has 4 classes. | Anyone running `test.py` on the current model would have gotten garbage class names (e.g. a 50 might have been printed as "100"). | Deleted `test.py`. Replaced it with a clean `predict.py` that reads class names from the loaded model and refuses to start if the names look wrong. |
| 6 | There was no production inference script — `test.py` was hardcoded to one image and one model path. | No clean way to run the model on new photos or batches without editing code. | Built `predict.py` with proper command-line arguments, folder/single-image input, configurable confidence and IoU thresholds, configurable device selection, graceful "no detections" handling, per-class detection summary, peso total, and annotated output. |

### Things that were NOT broken (ruled out during diagnosis)

These are worth listing so you don't waste time on them later:

- The trained weights themselves were correct — it learned to discriminate the four denominations almost perfectly.
- The augmented Roboflow dataset is internally consistent — every image of class index 0 actually shows the same denomination, every image of class index 1 actually shows the same denomination, etc. There were zero mixed-class label files.
- The training command worked correctly; the GPU was used; mixed precision (AMP) was enabled and gave the expected speedup.
- The validation metrics were genuine — they just happened to be reported under the wrong human-readable names. After fixing the names, the confusion matrix is purely diagonal.

---

## 5. The story of what was broken (so this doesn't happen again)

When training first finished, the model had perfect-looking validation scores (mAP50-95 of 98.4%), but on a real photo of a 50-peso bill it predicted "500" with 88% confidence. We diagnosed it like this:

1. **Pulled 60 random labeled images** from the training set, 15 per class, and rendered them with their bounding boxes.
2. **Discovered every image in "class 0" was actually a 100-peso bill**, every image in "class 1" was actually a 1000-peso bill, every image in "class 2" was actually a 50, and every image in "class 3" was actually a 500.
3. **Realized the cause:** Roboflow exports class names sorted alphabetically as strings. Because `'1000'` sorts before `'50'` in string order, the auto-generated `names: ['50', '100', '500', '1000']` line in `data.yaml` was wrong — the actual indices in the label files were `[100, 1000, 50, 500]`.
4. **The model itself was fine** — it had learned to map "this looks like a 50" to class index 2 correctly. Only the human-readable labels were shuffled. Validation passed because the val set had the same shuffled labels.

### The fix
- Updated `data.yaml` with the correct names (and a warning comment).
- Patched the trained model's embedded class-name dict, saved as `pesolens_best.pt`. **No retraining needed** — the weights were already correct.
- Re-validated: confusion matrix is purely diagonal across all 200 images.
- Re-tested the diagnostic image used during that fix: it reported the intended denomination after the class-name patch.
- Re-tested this checkout's current `test_bill.jpg` on 2026-05-22: it reports one P1000 and one P500 for a total of P1500 with the current defaults.

### The lesson for next time
**Whenever you export a fresh dataset from Roboflow, take 5 random images per class and visually verify the labels match before trusting the auto-generated class names.** The warning comment in `data.yaml` reminds you of this.

---

## 6. How to retrain (if you ever get more data)

For the current ESP32-S3 candidate, use the Kaggle workflow instead of the older local 4-class command:

```text
C:\Users\james\Projects\PesoLens-project\PesoLens-training\KAGGLE_RETRAIN_STEPS.md
```

Current canonical class order:

```text
20, 50, 100, 200, 500, 1000
```

Current firmware target:

```text
YOLOv8n, imgsz=224, INT8/full-integer TFLite
```

If the ESP32-S3 runtime is too slow, the preferred later path is a tiny 7-class INT8 classifier (`no_bill`, `20`, `50`, `100`, `200`, `500`, `1000`) instead of YOLO. The notes for that are in `firmware\FUTURE_CLASSIFIER_MODEL_PLAN.md`. A 160 YOLO export remains a fallback experiment.

### After training finishes
Save the new weights and result images under a clearly named run folder in `runs\`, then export INT8 TFLite in Kaggle.

**Important:** If you re-trained from a fresh Roboflow/Kaggle export, verify the class names before firmware embedding:

```python
from ultralytics import YOLO
m = YOLO("path/to/best.pt")
print(m.names)
# Should print: {0: '20 peso', 1: '50 peso', 2: '100 peso', 3: '200 peso', 4: '500 peso', 5: '1000 peso'}
```

Then update `firmware\include\config.h` to exactly match that order before flashing the new model.

### Adjusting training
- **More epochs** (`epochs=50`): slower, may overfit on a small dataset
- **Bigger batch** (`batch=32` or `batch=64`): the RTX 5090 can handle it, will train faster
- **Bigger images** (`imgsz=320` or above): may improve detection, but will likely be too slow/heavy for ESP32-S3 local inference
- **Lower starting LR** if it's plateauing: add `lr0=0.001`

---

## 7. If predictions look wrong

| Symptom | Likely cause | What to try |
|---------|--------------|-------------|
| No bills detected | Threshold too high, or photo is very different from training data | `--conf 0.2` |
| Detects same bill twice (overlapping boxes) | NMS overlap threshold too lax, or confidence threshold too low | Try `--conf 0.7` or a stricter `--iou 0.25` |
| Always predicts wrong denomination | The names mapping got shuffled again | See Section 5/6 |
| Confidence is always low (under 30%) | Image is blurry, dark, or bill is partially obscured | Improve photo, or retrain with similar images |
| Crashes with "CUDA out of memory" | Something else is using the GPU | Close LM Studio, browser games, etc. and retry |
| `Model file not found` | predict.py can't find `pesolens_best.pt` | Make sure you're running it from the project folder |
| `Model class names look wrong` | The loaded model has shuffled names | Re-run the model-name patching step from Section 5 |

---

## 8. Known limits and risks

1. **Dataset is class-imbalanced.** There are 642 images of 100s and 813 of 50s, but only 270 each of 500 and 1000. Validation passes for all four, but in unusual lighting / angles the rarer denominations are statistically more likely to fail first. If you collect more data, prioritize 500 and 1000.

2. **Validation isn't the same as the wild.** All 200 val images came from the same Roboflow augmentation pipeline as the training data. Real-world phone photos in unusual conditions may behave differently. The fix is to take 10-20 photos in your actual deployment conditions and run them through `predict.py`. If accuracy drops, retrain with those photos added.

3. **Currency redesign risk.** BSP occasionally redesigns the polymer notes. The training set looks like it includes 2024 and 2025 series. If BSP issues a redesigned bill, the model might miss it until you retrain.

4. **Single-side detection.** The model has been trained on both front and back of bills, which is good — but if you photograph a bill at a very steep angle (almost edge-on), it may fail.

5. **No counterfeit detection.** This model recognizes denominations only. It has no opinion on whether a bill is real or fake.

6. **Currency-only.** It does not detect coins, foreign currency, or non-currency objects. It will simply ignore them.

---

## 9. System requirements

This checkout was rechecked on 2026-05-22:

| Component | Version / spec |
|-----------|----------------|
| OS | Windows 11 Pro |
| Python | 3.12.10 |
| PyTorch | 2.11.0+cpu (`torch.cuda.is_available()` is `False`) |
| ultralytics | 8.4.48 |
| OpenCV | 4.13.0 |

The original training handover was done on an NVIDIA RTX 5090. That GPU is Blackwell architecture (sm_120) and needs PyTorch built against CUDA 12.8 or newer. If you move this project back to that GPU machine, the regular CPU PyTorch install will not use the GPU; install a CUDA-enabled PyTorch build and run inference with `--device 0`.

To check the current PyTorch runtime:
```powershell
.\.venv\Scripts\python.exe -c "import torch; print(torch.__version__, torch.cuda.is_available())"
```
For a GPU setup, `torch.cuda.is_available()` should print `True`.

---

## 10. Glossary (in case you ever talk to a developer about this)

- **YOLO** = the family of detection models PesoLens is built on. We're using YOLO v8.
- **Inference** = running the model to get a prediction (as opposed to "training" which is teaching the model).
- **Confidence threshold** = how sure the model has to be before it says it found something. Range 0-1.
- **mAP50-95** = the standard "how good is this detector overall" score. Range 0-1, higher is better. PesoLens scores 0.984.
- **Bounding box** = the rectangle drawn around each detected bill.
- **NMS / IoU threshold** = the rule for "if two boxes overlap a lot, they're probably the same bill — keep only one."
- **Epoch** = one pass through the entire training dataset.
- **Checkpoint / weights / .pt file** = the trained model saved to disk.
