"""
FARMEYE — Otomatik inek etiketi (YOLO txt)
==========================================
C:\\FarmEyeData\\inekfotograflari\\{yandan,arkadan,onden} içindeki
fotoğrafları yolov8n ile tarar; cow/sheep kutularını tek sınıf (0=inek)
olarak etiketler.

Çıktı:
  C:\\FarmEyeData\\inekfotograflari\\etiketler\\<alt>\\<ad>.txt
  C:\\FarmEyeData\\inekfotograflari\\etiketler\\_bos\\liste.txt  (kutu yok)

Kullanım:
  python scripts/auto_label_cows.py
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(r"C:\FarmEyeData\inekfotograflari")
LABELS = ROOT / "etiketler"
EMPTY = LABELS / "_bos"
PROJECT = Path(__file__).resolve().parents[1]
ONNX = PROJECT / "models" / "yolov8n.onnx"

# COCO: cow=19, sheep=18 (telefon/çiftlik karışımı)
COW_LIKE = {18, 19}
CONF = 0.25
IMG_EXT = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".JPG", ".JPEG", ".PNG"}


def letterbox(im, size=640):
    import cv2
    import numpy as np

    h, w = im.shape[:2]
    scale = min(size / h, size / w)
    nh, nw = int(round(h * scale)), int(round(w * scale))
    resized = cv2.resize(im, (nw, nh), interpolation=cv2.INTER_LINEAR)
    canvas = np.full((size, size, 3), 114, dtype=np.uint8)
    top = (size - nh) // 2
    left = (size - nw) // 2
    canvas[top : top + nh, left : left + nw] = resized
    return canvas, scale, left, top


def detect_opencv(net, im, conf=CONF):
    import cv2
    import numpy as np

    lb, scale, pad_x, pad_y = letterbox(im, 640)
    blob = cv2.dnn.blobFromImage(lb, 1 / 255.0, (640, 640), swapRB=True, crop=False)
    net.setInput(blob)
    outs = net.forward(net.getUnconnectedOutLayersNames())
    pred = outs[0]
    if pred.ndim == 3:
        pred = pred.reshape(pred.shape[1], pred.shape[2])
    # pred: (84, N) or (N, 84)
    if pred.shape[0] < pred.shape[1] and pred.shape[0] <= 84:
        # (C, N)
        rows, cols = pred.shape
        num_cls = rows - 4
        boxes, scores, classes = [], [], []
        for i in range(cols):
            cls_scores = pred[4:, i]
            c = int(np.argmax(cls_scores))
            s = float(cls_scores[c])
            if s < conf or c not in COW_LIKE:
                continue
            cx, cy, w, h = pred[0:4, i]
            boxes.append([cx - w / 2, cy - h / 2, w, h])
            scores.append(s)
            classes.append(c)
    else:
        # (N, C)
        boxes, scores, classes = [], [], []
        for row in pred:
            cls_scores = row[4:]
            c = int(np.argmax(cls_scores))
            s = float(cls_scores[c])
            if s < conf or c not in COW_LIKE:
                continue
            cx, cy, w, h = row[0:4]
            boxes.append([cx - w / 2, cy - h / 2, w, h])
            scores.append(s)
            classes.append(c)

    if not boxes:
        return []

    idxs = cv2.dnn.NMSBoxes(
        [[int(b[0]), int(b[1]), int(b[2]), int(b[3])] for b in boxes],
        scores,
        conf,
        0.45,
    )
    if idxs is None or len(idxs) == 0:
        return []

    h0, w0 = im.shape[:2]
    out = []
    for i in np.array(idxs).reshape(-1):
        x, y, w, h = boxes[i]
        # letterbox -> original
        x = (x - pad_x) / scale
        y = (y - pad_y) / scale
        w = w / scale
        h = h / scale
        x1 = max(0.0, x)
        y1 = max(0.0, y)
        x2 = min(float(w0), x + w)
        y2 = min(float(h0), y + h)
        bw = x2 - x1
        bh = y2 - y1
        if bw < 2 or bh < 2:
            continue
        # YOLO normalized
        xc = ((x1 + x2) / 2) / w0
        yc = ((y1 + y2) / 2) / h0
        nw = bw / w0
        nh = bh / h0
        out.append((0, xc, yc, nw, nh, scores[i]))
    return out


def detect_ultralytics(model, path, conf=CONF):
    results = model.predict(str(path), conf=conf, verbose=False, classes=list(COW_LIKE))
    out = []
    for r in results:
        if r.boxes is None:
            continue
        h0, w0 = r.orig_shape[:2]
        for b in r.boxes:
            x1, y1, x2, y2 = b.xyxy[0].tolist()
            xc = ((x1 + x2) / 2) / w0
            yc = ((y1 + y2) / 2) / h0
            nw = (x2 - x1) / w0
            nh = (y2 - y1) / h0
            out.append((0, xc, yc, nw, nh, float(b.conf[0])))
    return out


def write_yolo(path: Path, dets):
    lines = [f"{c} {xc:.6f} {yc:.6f} {w:.6f} {h:.6f}" for c, xc, yc, w, h, _ in dets]
    path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")


def main():
    import cv2
    import numpy as np  # noqa: F401

    LABELS.mkdir(parents=True, exist_ok=True)
    EMPTY.mkdir(parents=True, exist_ok=True)

    use_ultra = False
    ultra_model = None
    net = None
    try:
        from ultralytics import YOLO

        ultra_model = YOLO("yolov8n.pt")  # downloads if needed; COCO
        use_ultra = True
        print("Backend: ultralytics yolov8n.pt")
    except Exception as e:
        print("ultralytics yok, OpenCV DNN kullanılıyor:", e)
        if not ONNX.exists():
            print("ONNX bulunamadı:", ONNX)
            return 1
        net = cv2.dnn.readNetFromONNX(str(ONNX))
        net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
        net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)
        print("Backend: OpenCV DNN", ONNX)

    # alt klasörler: yandan, arkadan, onden (+ bozuk önden)
    subdirs = [p for p in ROOT.iterdir() if p.is_dir() and p.name not in ("etiketler",)]
    images = []
    for d in subdirs:
        for f in d.rglob("*"):
            if f.is_file() and f.suffix in IMG_EXT:
                images.append(f)

    print(f"Foto sayısı: {len(images)}")
    labeled = 0
    empty = 0
    empty_list = []

    for i, img_path in enumerate(sorted(images), 1):
        rel_parent = img_path.parent.name
        out_dir = LABELS / rel_parent
        out_dir.mkdir(parents=True, exist_ok=True)
        out_txt = out_dir / (img_path.stem + ".txt")

        try:
            if use_ultra:
                dets = detect_ultralytics(ultra_model, img_path)
            else:
                im = cv2.imread(str(img_path))
                if im is None:
                    empty += 1
                    empty_list.append(str(img_path))
                    continue
                dets = detect_opencv(net, im)
        except Exception as ex:
            print("HATA", img_path.name, ex)
            empty += 1
            empty_list.append(str(img_path))
            continue

        write_yolo(out_txt, dets)
        if dets:
            labeled += 1
        else:
            empty += 1
            empty_list.append(str(img_path))
            # boş txt de yazıldı — ayrıca liste
        if i % 20 == 0 or i == len(images):
            print(f"  [{i}/{len(images)}] labeled={labeled} empty={empty}")

    (EMPTY / "liste.txt").write_text("\n".join(empty_list) + "\n", encoding="utf-8")
    classes = LABELS / "classes.txt"
    classes.write_text("inek\n", encoding="utf-8")

    print("==== ÖZET ====")
    print(f"Toplam foto : {len(images)}")
    print(f"Etiketli    : {labeled}")
    print(f"Boş/kaçıran : {empty}")
    print(f"Çıktı       : {LABELS}")
    print(f"Boş liste   : {EMPTY / 'liste.txt'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
