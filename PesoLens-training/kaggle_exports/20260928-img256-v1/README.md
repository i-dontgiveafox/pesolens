# PesoLens imgsz-256 retrain — Kaggle Version 1 (2026-09-28)

Kernel: https://www.kaggle.com/code/jamesasperilla/pesolens-img256 (Version 1, script version id 353627617)
Status on Kaggle: "failed" — but ONLY the post-training export check failed. Training completed.

## Verified (2026-09-29)
- 100/100 epochs (train_results inside best.pt records epochs 1..100; results.csv has 100 rows; 4.21 h on 2x T4).
- best.pt = best-fitness epoch 89. sha256 cfa7b65d49661e6fc1968917c7034e5363ad0d9e34eb50913371d47711ff5efe
  (identical to C:\Users\james\Downloads\best.zip, which is best.pt with a .zip extension).
- "epoch: -1" inside best.pt is normal: Ultralytics sets it when it strips the optimizer after training.
- Classes {0:'20',1:'50',2:'100',3:'500',4:'1000'}; Ultralytics 8.4.90; imgsz 256; all other args = approved 320 run.
- Val (567 images): mAP50 0.962, mAP50-95 0.818, P 0.949, R 0.938  (approved 320 model: 0.969 / 0.834 / 0.976 / 0.931).

## Do NOT flash weights/best_int8.tflite
Ultralytics 8.4.90 exported it as float32 NCHW [1,3,256,256]; the firmware needs int8 NHWC [1,256,256,3],
scale 1/255, zero point -128. Re-export best.pt with Ultralytics 8.4.76 (int8=True), then firmware/tools/convert_model.py.

## Contents
runs/.../weights/best.pt, last.pt, best_int8.tflite (unusable), results.csv/png, args.yaml, val_metrics.json,
confusion_matrix(.png, _normalized.png), P/R/F1/PR curves, labels.jpg, train/val batch previews,
kaggle_v1_run.log (full Kaggle log), kernel-metadata.json.
Not included: pesolens_merged/ (~8 GB copy of the training dataset; the originals are in Downloads).
