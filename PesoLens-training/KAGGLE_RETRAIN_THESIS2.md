# PesoLens Kaggle Retrain — Thesis 2 Roboflow zips

Created: 2026-07-07

This is the from-scratch Kaggle notebook for retraining PesoLens on the two
Roboflow exports:

```
Thesis 2.yolov8 (1).zip
Thesis 2 additional set.yolov8.zip
```

## Source audit (done locally 2026-07-07)

Both zips share the **identical 9-class scheme in identical order** (no Roboflow
alphabetical-order mismatch between them):

```
0 new_100   3 new_500   6 old_20
1 new_1000  4 old_100   7 old_50
2 new_50    5 old_1000  8 old_500
```

These 9 classes are new-series vs old-series of 5 denominations. There is **no
P200**. We roll them up 9 -> 5 by denomination into the firmware order
`[20, 50, 100, 500, 1000]`.

Image counts (Roboflow splits preserved):

| split | ZIP1 | ZIP2 | merged |
|-------|------|------|--------|
| train | 1890 | 2001 | 3891   |
| valid |  405 |  162 |  567   |
| test  |  405 |  161 |  566   |
| total | 2700 | 2324 | 5024   |

Combined denomination object counts (the assert target after remap):

| idx | denom | count |
|-----|-------|-------|
| 0 | 20   | 644   |
| 1 | 50   | 1675  |
| 2 | 100  | 1582  |
| 3 | 500  | 1377  |
| 4 | 1000 | 1374  |

**P500 now has 1,377 instances** (previous merged set had ~150). This directly
addresses the old P500 recall weakness (0.41).

709 images are real HEIC (51 in ZIP1, 658 in ZIP2) and are converted to JPG
during the merge. 90 jpeg + 190 png in ZIP2 are copied as-is.

---

## Kaggle setup (before running cells)

1. Kaggle -> **Datasets -> New Dataset**. Upload BOTH zips into one private
   dataset. Name it e.g. `pesolens-thesis2`.
2. Open a **New Notebook**. Right panel -> **Add Input** -> add that dataset.
3. Settings -> **Accelerator = GPU T4 x1**. Internet ON (to fetch yolov8n.pt).
4. The zips will appear under `/kaggle/input/<your-dataset-slug>/`.

Run the cells below top to bottom.

---

## Cell 1 — install

```python
!pip install -q ultralytics pillow-heif
```

## Cell 2 — imports + config

```python
from pathlib import Path
from collections import Counter
import shutil, zipfile
from PIL import Image
from pillow_heif import register_heif_opener
register_heif_opener()

# Firmware class order — DO NOT reorder.
TARGET = ['20', '50', '100', '500', '1000']
target_index = {d: i for i, d in enumerate(TARGET)}

# Roboflow 9-class index -> denomination string
NINE_TO_DENOM = {
    0: '100', 1: '1000', 2: '50', 3: '500',
    4: '100', 5: '1000', 6: '20', 7: '50', 8: '500',
}
REMAP = {i: target_index[NINE_TO_DENOM[i]] for i in range(9)}
print('9->5 label remap:', REMAP)  # {0:2,1:4,2:1,3:3,4:2,5:4,6:0,7:1,8:3}

OUT = Path('/kaggle/working/pesolens_merged')
IMG_SIZE = 320
```

## Cell 3 — find the two zips

```python
zips = sorted(Path('/kaggle/input').rglob('*.zip'))
print('Found zips:')
for z in zips:
    print(' ', z)
assert len(zips) == 2, f'Expected 2 zips, found {len(zips)}: {zips}'
```

## Cell 4 — merge + remap + HEIC->JPG

```python
if OUT.exists():
    shutil.rmtree(OUT)
for split in ['train', 'valid', 'test']:
    (OUT / split / 'images').mkdir(parents=True, exist_ok=True)
    (OUT / split / 'labels').mkdir(parents=True, exist_ok=True)

IMG_EXT = {'.jpg', '.jpeg', '.png', '.bmp', '.webp', '.heic', '.heif'}

def remap_label(text: str) -> str:
    out = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        parts[0] = str(REMAP[int(float(parts[0]))])
        out.append(' '.join(parts))
    return ('\n'.join(out) + '\n') if out else ''

for zi, zpath in enumerate(zips):
    prefix = f'z{zi}'
    raw = Path(f'/kaggle/working/_raw_{prefix}')
    if raw.exists():
        shutil.rmtree(raw)
    with zipfile.ZipFile(zpath) as zf:
        zf.extractall(raw)
    # data.yaml sits at the export root
    root = next(raw.rglob('data.yaml')).parent
    for split in ['train', 'valid', 'test']:
        idir = root / split / 'images'
        ldir = root / split / 'labels'
        if not idir.exists():
            continue
        for img in idir.iterdir():
            if img.suffix.lower() not in IMG_EXT:
                continue
            stem = img.stem
            if img.suffix.lower() in {'.heic', '.heif'}:
                dst_img = OUT / split / 'images' / f'{prefix}_{stem}.jpg'
                Image.open(img).convert('RGB').save(dst_img, quality=92)
            else:
                dst_img = OUT / split / 'images' / f'{prefix}_{stem}{img.suffix.lower()}'
                shutil.copy2(img, dst_img)
            lbl = ldir / f'{stem}.txt'
            dst_lbl = OUT / split / 'labels' / f'{prefix}_{stem}.txt'
            dst_lbl.write_text(remap_label(lbl.read_text()) if lbl.exists() else '')
    shutil.rmtree(raw)

print('Merge done.')
```

## Cell 5 — audit (safety gate — do not train if this fails)

```python
EXPECTED_IMAGES = {'train': 3891, 'valid': 567, 'test': 566}
EXPECTED_HIST = Counter({0: 644, 1: 1675, 2: 1582, 3: 1377, 4: 1374})

hist = Counter()
for split in ['train', 'valid', 'test']:
    imgs = list((OUT / split / 'images').glob('*'))
    lbls = list((OUT / split / 'labels').glob('*.txt'))
    print(f'{split:5s}: {len(imgs)} imgs, {len(lbls)} labels')
    assert len(imgs) == EXPECTED_IMAGES[split], (split, len(imgs))
    for lp in lbls:
        for line in lp.read_text().splitlines():
            if line.strip():
                hist[int(line.split()[0])] += 1

print('Class histogram:', {TARGET[k]: hist[k] for k in sorted(hist)})
assert hist == EXPECTED_HIST, (hist, EXPECTED_HIST)
print('AUDIT PASSED — safe to train.')
```

## Cell 6 — write data.yaml

```python
import yaml
data = {
    'path': str(OUT),
    'train': 'train/images',
    'val': 'valid/images',
    'test': 'test/images',
    'nc': len(TARGET),
    'names': TARGET,
}
(OUT / 'data.yaml').write_text(yaml.safe_dump(data, sort_keys=False))
print((OUT / 'data.yaml').read_text())
```

## Cell 7 — visual label sanity check (optional but recommended)

```python
import random
import cv2
import numpy as np
from IPython.display import display

def draw(img_path, lbl_path):
    im = cv2.cvtColor(cv2.imread(str(img_path)), cv2.COLOR_BGR2RGB)
    h, w = im.shape[:2]
    for line in Path(lbl_path).read_text().splitlines():
        if not line.strip():
            continue
        c, x, y, bw, bh = line.split()[:5]
        c = int(c); x=float(x)*w; y=float(y)*h; bw=float(bw)*w; bh=float(bh)*h
        p1 = (int(x-bw/2), int(y-bh/2)); p2 = (int(x+bw/2), int(y+bh/2))
        cv2.rectangle(im, p1, p2, (255,0,0), 3)
        cv2.putText(im, f'P{TARGET[c]}', (p1[0], max(0,p1[1]-8)),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.2, (255,0,0), 3)
    return Image.fromarray(im).resize((400, 300))

samples = list((OUT/'train'/'images').glob('*'))
for img in random.sample(samples, 8):
    lbl = OUT/'train'/'labels'/f'{img.stem}.txt'
    display(draw(img, lbl))
    print(img.name, '->', lbl.read_text().strip()[:80])
```

Eyeball these: the drawn `P50 / P100 / P500 / P1000 / P20` label must match the
bill in the picture. If any is wrong, STOP — the remap is off.

## Cell 8 — train (full 100 epochs)

```python
from ultralytics import YOLO

model = YOLO('yolov8n.pt')
results = model.train(
    data=str(OUT / 'data.yaml'),
    imgsz=IMG_SIZE,        # 320 — firmware MODEL_INPUT_SIZE
    epochs=100,
    patience=100,          # never early-stop; run the full 100
    batch=16,              # T4 handles 16 at imgsz 320
    device=0,
    workers=2,
    project='/kaggle/working/runs',
    name='pesolens_5class_yolov8n_img320_thesis2',
    # augmentation — tuned for UNCONTROLLED bill orientation on an ESP32 camera.
    # Bills arrive at any angle (assistive use), so full rotation invariance.
    # Hue jitter kept LOW on purpose: denomination is color-coded
    # (20 orange, 50 red, 100 violet, 500 yellow, 1000 blue) and heavy hue
    # shifting would erase the main cue.
    fliplr=0.5,
    flipud=0.5,            # any orientation
    degrees=180.0,         # full in-plane rotation invariance
    translate=0.1,         # bill not always centered
    scale=0.5,             # camera-to-bill distance varies
    hsv_h=0.01,            # keep LOW — color = denomination
    hsv_s=0.3, hsv_v=0.3,
    mosaic=1.0, close_mosaic=10,
)
run_dir = Path(model.trainer.save_dir)
best_pt = run_dir / 'weights' / 'best.pt'
assert best_pt.is_file(), best_pt
print('RUN:', run_dir)
print('BEST:', best_pt)
```

## Cell 9 — validate + verify class order

```python
m = YOLO(str(best_pt))
print('names:', m.names)
assert [str(m.names[i]) for i in range(5)] == TARGET, m.names
metrics = m.val(data=str(OUT / 'data.yaml'), imgsz=IMG_SIZE, device=0)
# Watch per-class recall in the printed table — P500 should now be strong.
```

## Cell 10 — export INT8 TFLite

```python
exported = m.export(
    format='tflite',
    int8=True,
    imgsz=IMG_SIZE,
    data=str(OUT / 'data.yaml'),
)
print('Exported to:', exported)
for p in Path('/kaggle/working').rglob('*.tflite'):
    print(f'{p.stat().st_size/1024/1024:.2f} MB  {p}')
```

## Cell 11 — package everything for download

```python
from IPython.display import FileLink

pkg = Path('/kaggle/working/pesolens_thesis2_package')
if pkg.exists():
    shutil.rmtree(pkg)
pkg.mkdir()

shutil.copy2(best_pt, pkg / 'best.pt')
shutil.copy2(OUT / 'data.yaml', pkg / 'data.yaml')

exp = Path(exported)
exp_dir = exp if exp.is_dir() else exp.parent
for p in exp_dir.glob('*'):
    if p.is_file():
        shutil.copy2(p, pkg / p.name)
for name in ['results.png', 'results.csv', 'confusion_matrix.png',
             'confusion_matrix_normalized.png', 'BoxPR_curve.png', 'args.yaml']:
    src = run_dir / name
    if src.is_file():
        shutil.copy2(src, pkg / name)

zip_path = shutil.make_archive('/kaggle/working/pesolens_thesis2_export', 'zip', pkg)
print('ZIP:', zip_path)
FileLink('/kaggle/working/pesolens_thesis2_export.zip')
```

**Before closing Kaggle:** download that zip AND click **Save Version** (with
output) so `/kaggle/working` is not lost — this project has lost a Kaggle
session before.

---

## Local integration (after download)

1. Unzip the package locally, e.g. into
   `PesoLens-training/kaggle_exports/<date>/`.
2. Copy `best.pt` -> `PesoLens-training/pesolens_best.pt` (the file
   `predict.py` loads).
3. Verify names, then test:

```powershell
cd "C:\Users\james\Projects\PesoLens-project\PesoLens-training"
.\.venv\Scripts\python.exe -c "from ultralytics import YOLO; print(YOLO('pesolens_best.pt').names)"
.\.venv\Scripts\python.exe predict.py test_bill.jpg
```

Expected names: `{0:'20',1:'50',2:'100',3:'500',4:'1000'}`. `predict.py` already
expects exactly `{20,50,100,500,1000}` and will refuse to run otherwise.

4. Firmware (only if reflashing): copy `best_full_integer_quant.tflite` into a
   new folder under `firmware/model_exports/` and follow the embedding /
   acceptance steps in `KAGGLE_RETRAIN_STEPS.md`. Firmware already expects
   5 classes at input size 320 (`MODEL_INPUT_SIZE=320`, `MODEL_NUM_CLASSES=5`).
</content>
</invoke>
