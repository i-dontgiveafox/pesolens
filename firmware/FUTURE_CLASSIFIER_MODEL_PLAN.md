# Future Classifier Model Plan

Status: deferred at the 2026-05-30 pause checkpoint. Finish the current D6
manual-capture firmware tests first, then return to this model replacement plan.

The current ESP32-S3 firmware uses a YOLOv8n object-detection model at
224x224 input. It works, but on-device inference takes about 32 seconds per
scan. That is too slow for the intended assistive use case.

Later, replace YOLO with a small full-integer INT8 image classifier. The user
will present one bill at a time, so the firmware does not need bounding boxes,
anchors, or NMS. It only needs to answer one label:

```text
no_bill
20
50
100
200
500
1000
```

Recommended target:

```text
Model type: tiny CNN or MobileNet-style classifier
Input: 96x96 or 128x128 RGB
Output: 7 classes
Export: full-integer INT8 .tflite
Firmware flow: D6 press -> 3s delay -> capture -> classify -> speak result
```

Expected benefit:

- Much less compute than YOLO.
- No detection post-processing.
- Better fit for ESP32-S3 if only one bill is shown at a time.

Keep the current YOLO model only as a proof of concept until the classifier is
trained and verified.
