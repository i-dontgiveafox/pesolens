# Validate the deployed model on the locally-rebuilt 5-class valid split and
# print the per-class mAP table. Run build_local_val_set.py FIRST.
#
# Run:  .\.venv\Scripts\python.exe run_local_val.py
# See VALIDATION_METRICS_20260727.md for the reference numbers to compare against.

from pathlib import Path
from ultralytics import YOLO

HERE = Path(__file__).resolve().parent
DATA = HERE / "local_val_dataset" / "data.yaml"
MODEL = HERE / "pesolens_best.pt"
PROJECT = HERE / "local_val_runs"

m = YOLO(str(MODEL))
print("names:", m.names)
metrics = m.val(
    data=str(DATA), split="val", imgsz=320, device="cpu",
    project=str(PROJECT), name="pesolens_val", exist_ok=True,
    plots=True, verbose=True,
)
print("\n=== SUMMARY (all classes) ===")
print("precision:", round(float(metrics.box.mp), 4))
print("recall:   ", round(float(metrics.box.mr), 4))
print("mAP50:    ", round(float(metrics.box.map50), 4))
print("mAP50-95: ", round(float(metrics.box.map), 4))
print("plots + results saved to:", metrics.save_dir)
