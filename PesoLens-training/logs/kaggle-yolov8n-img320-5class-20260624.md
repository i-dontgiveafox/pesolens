# Kaggle YOLOv8n 5-Class Run Log

Date recorded: 2026-06-24

## Run

```text
Run directory:
/kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_022538

Best checkpoint:
/kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_022538/weights/best.pt

Last checkpoint:
/kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_022538/weights/last.pt
```

This run was configured for:

```text
Model: YOLOv8n
Classes: 20, 50, 100, 500, 1000
Image size: 320
Requested epochs: 100
EarlyStopping patience: 20
```

## Training Result

Training stopped at epoch 81 because YOLO EarlyStopping detected no improvement
for 20 epochs after the best result.

```text
EarlyStopping: Training stopped early as no improvement observed in last 20 epochs.
Best results observed at epoch 61, best model saved as best.pt.
81 epochs completed in 0.763 hours.
```

This means training completed normally. It did not crash, and Kaggle did not
fail. The trainer was allowed to run up to 100 epochs, but it automatically
stopped at 81 because continuing to 100 was unlikely to improve the model.

The best model is `best.pt`, not `best.py`. YOLO saves PyTorch model weights as
`.pt` files.

## Final Validation From `best.pt`

```text
Ultralytics 8.4.75  Python-3.12.13 torch-2.10.0+cu128 CUDA:0 (Tesla T4, 14912MiB)
Model summary (fused): 73 layers, 3,006,623 parameters, 0 gradients, 8.1 GFLOPs

Class     Images  Instances   Box(P)      R      mAP50  mAP50-95
all          405        406    0.836   0.829      0.915     0.769
20            45         45    0.690   0.911      0.905     0.773
50            90         91    0.919   0.978      0.973     0.808
100           90         90    0.939   0.956      0.981     0.786
500           90         90    0.815   0.411      0.772     0.661
1000          90         90    0.817   0.890      0.944     0.818

Speed: 0.1ms preprocess, 1.3ms inference, 0.0ms loss, 1.8ms postprocess per image
```

Overall validation is strong:

```text
mAP50:    0.915
mAP50-95: 0.769
Precision: 0.836
Recall:    0.829
```

Main issue to watch:

```text
500-peso recall: 0.411
```

The model is reasonably precise when it predicts `500`, but it misses many real
`500` examples. Before using this in firmware, test the model on held-out
camera images, especially 500-peso bills.

## Kaggle Output Note

The files were saved in `/kaggle/working`, not necessarily shown immediately in
the notebook Output panel.

Use this in Kaggle to inspect the saved weights:

```python
!ls -lah /kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_022538/weights
```

Use this to package the run for download:

```python
!zip -r /kaggle/working/pesolens_5class_yolov8n_img320_e100_20260624_022538.zip /kaggle/working/runs/pesolens_5class_yolov8n_img320_e100_20260624_022538
```

Then create a download link:

```python
from IPython.display import FileLink
FileLink("/kaggle/working/pesolens_5class_yolov8n_img320_e100_20260624_022538.zip")
```

If the Kaggle session is already gone and the notebook version was not saved,
files under `/kaggle/working` may be lost and the run may need to be repeated.

## Easy Explanation To Share

I set the training to 100 epochs, but YOLO has EarlyStopping enabled. That means
it can stop before 100 if the model stops improving. In this run, the best model
was found at epoch 61, then YOLO waited 20 more epochs. Since there was no better
result, it stopped at epoch 81 and saved the best model as `best.pt`. So the
100-epoch setup worked; it just did not need to use all 100 epochs.

The training completed successfully. It did not fail. The final best model got
about 91.5% mAP50 overall, but I still need to test it more because the 500-peso
class has low recall and may miss some 500-peso bills.

## Next Actions

1. Download or save the Kaggle run package before the Kaggle session is lost.
2. Keep `best.pt` as the training checkpoint.
3. Export and package the TFLite model only after verifying class order.
4. Test on held-out ESP32 camera images, with extra attention to 500-peso bills.
