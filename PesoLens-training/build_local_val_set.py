# Rebuild the 5-class VALID split of the "thesis2" dataset locally, from the raw
# Thesis 2 Roboflow exports in Downloads. Produces a dataset whose class order
# matches the deployed model {0:20,1:50,2:100,3:500,4:1000} so model.val() gives
# valid per-class metrics. See VALIDATION_METRICS_20260727.md.
#
# Run:  .\.venv\Scripts\python.exe build_local_val_set.py
# Needs: pillow-heif  (pip install pillow-heif)

from pathlib import Path
from collections import Counter
import shutil
from PIL import Image
from pillow_heif import register_heif_opener
register_heif_opener()

# Firmware/model class order — DO NOT reorder.
TARGET = ['20', '50', '100', '500', '1000']
target_index = {d: i for i, d in enumerate(TARGET)}
NINE_TO_DENOM = {
    0: '100', 1: '1000', 2: '50', 3: '500',
    4: '100', 5: '1000', 6: '20', 7: '50', 8: '500',
}
REMAP = {i: target_index[NINE_TO_DENOM[i]] for i in range(9)}
print('9->5 label remap:', REMAP)

SOURCES = [
    Path(r"C:\Users\james\Downloads\Thesis 2.yolov8 (1)"),
    Path(r"C:\Users\james\Downloads\Thesis 2 additional set.yolov8"),
]
OUT = Path(__file__).resolve().parent / "local_val_dataset"
SPLIT = 'valid'   # change to 'test' for the held-out test split (566 images)
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

if OUT.exists():
    shutil.rmtree(OUT)
(OUT / SPLIT / 'images').mkdir(parents=True, exist_ok=True)
(OUT / SPLIT / 'labels').mkdir(parents=True, exist_ok=True)

n_img = n_heic = 0
for zi, root in enumerate(SOURCES):
    prefix = f'z{zi}'
    idir = root / SPLIT / 'images'
    ldir = root / SPLIT / 'labels'
    assert idir.exists(), idir
    for img in sorted(idir.iterdir()):
        if img.suffix.lower() not in IMG_EXT:
            continue
        stem = img.stem
        if img.suffix.lower() in {'.heic', '.heif'}:
            dst_img = OUT / SPLIT / 'images' / f'{prefix}_{stem}.jpg'
            Image.open(img).convert('RGB').save(dst_img, quality=92)
            n_heic += 1
        else:
            dst_img = OUT / SPLIT / 'images' / f'{prefix}_{stem}{img.suffix.lower()}'
            shutil.copy2(img, dst_img)
        lbl = ldir / f'{stem}.txt'
        dst_lbl = OUT / SPLIT / 'labels' / f'{prefix}_{stem}.txt'
        dst_lbl.write_text(remap_label(lbl.read_text()) if lbl.exists() else '')
        n_img += 1

hist = Counter()
for lp in (OUT / SPLIT / 'labels').glob('*.txt'):
    for line in lp.read_text().splitlines():
        if line.strip():
            hist[int(line.split()[0])] += 1

import yaml
data = {
    'path': str(OUT),
    'train': f'{SPLIT}/images',
    'val': f'{SPLIT}/images',
    'test': f'{SPLIT}/images',
    'nc': len(TARGET),
    'names': TARGET,
}
(OUT / 'data.yaml').write_text(yaml.safe_dump(data, sort_keys=False))
print(f'built {SPLIT}: images={n_img} (heic_converted={n_heic})')
print('class histogram:', {TARGET[k]: hist[k] for k in sorted(hist)})
print('data.yaml at:', OUT / 'data.yaml')
