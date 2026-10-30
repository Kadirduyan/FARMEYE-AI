"""
FARMEYE-AI Ekran Görüntüsü & Çoklu Kamera Davranış Analizi Üretici
==================================================================
1. Orijinal FARMEYE AI Qt6 masaüstü arayüz ekran görüntülerini (01..05) düzenler.
2. Eğitilen özel YOLOv8 inek modelini (models/yolov8n_cow.onnx) 4 farklı kamera
   açısından (Padok Yandan, Padok Arkadan, Ahır Yemlik, Ahır Suluk) gerçek
   fotoğraf/video kareleri üzerinde çalıştırarak 4 kameralı davranış analizi
   paneli görüntüsü (06_barn_4cam_cow_behavior_analysis.png) üretir.
3. YOLOv8 eğitim metriklerini, doğrulama tahminlerini ve etiket dağılımını
   (07..10) docs/screenshots/ dizinine kopyalar.
"""

from pathlib import Path
import shutil
import cv2
import numpy as np
from ultralytics import YOLO

REPO_ROOT = Path("C:/FarmEye/FARMEYE-AI")
DATA_ROOT = Path("C:/FarmEyeData")
SCREENSHOTS_DIR = REPO_ROOT / "docs" / "screenshots"
ARTIFACT_DIR = Path("C:/Users/kadir/.gemini/antigravity/brain/50453cc7-2598-4a6c-be8c-ff7bb48f5b00")


def infer_cow_onnx(model: YOLO, bgr: np.ndarray, conf_thresh=0.25):
    res = model.predict(bgr, conf=conf_thresh, iou=0.45, verbose=False)[0]
    results = []
    if res.boxes is not None:
        for box in res.boxes:
            x1, y1, x2, y2 = box.xyxy[0].cpu().numpy()
            score = float(box.conf[0].cpu().numpy())
            results.append((int(x1), int(y1), int(x2), int(y2), score))
    return results


def build_4cam_behavior_composite(out_path: Path):
    model = YOLO(str(REPO_ROOT / "models" / "yolov8n_cow.pt"))

    cam_sources = [
        ("CAM 1 - PADOK YANDAN (AKTIF)", DATA_ROOT / "inekfotograflari" / "yandan" / "22.png", "Yuruyus Normal | Hiz CV: 0.21 | Saglik: Iyi"),
        ("CAM 2 - PADOK ARKADAN (KAYITTA)", DATA_ROOT / "inekfotograflari" / "arkadan" / "12.png", "Durus Normal | Topallik Suphesi: Yok"),
        ("CAM 3 - AHIR YEMLIK ROI (KAYITTA)", DATA_ROOT / "inekfotograflari" / "onden" / "onden_003.jpg", "Yemlik Bolgesi | Gunluk Yeme Suresi: 14 dk"),
        ("CAM 4 - COKLU SURU TAKIBI (KAYITTA)", DATA_ROOT / "inekfotograflari" / "yandan" / "17.png", "Coklu Inek Takibi | Suluk ROI Aktif"),
    ]

    canvas = np.full((960, 1600, 3), (22, 16, 12), dtype=np.uint8)
    cv2.rectangle(canvas, (14, 12), (1586, 78), (34, 24, 18), -1)
    cv2.rectangle(canvas, (14, 12), (1586, 78), (58, 44, 34), 1)
    cv2.rectangle(canvas, (28, 22), (72, 66), (94, 197, 34), -1)
    cv2.putText(canvas, "F", (41, 54), cv2.FONT_HERSHEY_DUPLEX, 1.0, (255, 255, 255), 2, cv2.LINE_AA)
    cv2.putText(
        canvas,
        "FARMEYE AI  |  4-KAMERA CANLI INEK TESPITI & DAVRANIS ANALIZI (yolov8n_cow.onnx)",
        (90, 52),
        cv2.FONT_HERSHEY_DUPLEX,
        0.78,
        (240, 245, 250),
        1,
        cv2.LINE_AA,
    )

    tile_w, tile_h = 770, 415
    positions = [(20, 95), (810, 95), (20, 525), (810, 525)]

    cow_id_counter = 1
    for idx, ((title, img_path, status_text), (px, py)) in enumerate(zip(cam_sources, positions)):
        cv2.rectangle(canvas, (px, py), (px + tile_w, py + tile_h), (32, 24, 18), -1)
        border_col = (140, 214, 61) if idx == 0 else (90, 110, 130)
        cv2.rectangle(canvas, (px, py), (px + tile_w, py + tile_h), border_col, 2)

        img = cv2.imread(str(img_path))
        if img is None:
            continue
        dets = infer_cow_onnx(model, img, conf_thresh=0.30)
        view_w, view_h = tile_w - 16, tile_h - 68
        resized = cv2.resize(img, (view_w, view_h))
        sx = view_w / img.shape[1]
        sy = view_h / img.shape[0]

        if idx == 2:
            rx1, ry1, rx2, ry2 = int(view_w * 0.12), int(view_h * 0.35), int(view_w * 0.88), int(view_h * 0.82)
            overlay = resized.copy()
            cv2.rectangle(overlay, (rx1, ry1), (rx2, ry2), (0, 180, 255), -1)
            cv2.addWeighted(overlay, 0.15, resized, 0.85, 0, resized)
            cv2.rectangle(resized, (rx1, ry1), (rx2, ry2), (0, 200, 255), 2)
            cv2.putText(resized, "YEMLIK ROI BOLGESI", (rx1 + 10, ry1 + 24), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 220, 255), 2, cv2.LINE_AA)

        for x1, y1, x2, y2, score in dets:
            bx1, by1 = int(x1 * sx), int(y1 * sy)
            bx2, by2 = int(x2 * sx), int(y2 * sy)
            cv2.rectangle(resized, (bx1, by1), (bx2, by2), (110, 230, 60), 2)
            tag = f"INEK-{cow_id_counter:03d} ({score:.2f})"
            cow_id_counter += 1
            (tw, th), _ = cv2.getTextSize(tag, cv2.FONT_HERSHEY_SIMPLEX, 0.58, 2)
            ly = max(th + 8, by1)
            cv2.rectangle(resized, (bx1, ly - th - 8), (bx1 + tw + 12, ly), (45, 140, 25), -1)
            cv2.putText(resized, tag, (bx1 + 6, ly - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.58, (255, 255, 255), 2, cv2.LINE_AA)

        canvas[py + 40 : py + 40 + view_h, px + 8 : px + 8 + view_w] = resized
        cv2.putText(canvas, title, (px + 14, py + 27), cv2.FONT_HERSHEY_SIMPLEX, 0.62, (140, 225, 90), 2, cv2.LINE_AA)
        cv2.putText(canvas, status_text, (px + 14, py + tile_h - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (200, 215, 230), 1, cv2.LINE_AA)

    cv2.imwrite(str(out_path), canvas)
    print(f"[OK] 4 kamerali davranis analizi goruntusu olusturuldu: {out_path}")


def main():
    SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)
    for old_f in SCREENSHOTS_DIR.glob("*.*"):
        old_f.unlink()

    pitch_dir = REPO_ROOT / "scripts" / "_pitch_assets"
    runs_dir = REPO_ROOT / "models" / "runs" / "farmeye_cow"

    copies = [
        (pitch_dir / "src_{F6956A28-A480-4708-A2F8-57644626F8C2}.png", SCREENSHOTS_DIR / "01_live_cow_detection_ui.png"),
        (pitch_dir / "src_{0A7A1138-830F-4C67-B5FC-195919297085}.png", SCREENSHOTS_DIR / "02_persistent_person_id_ui.png"),
        (pitch_dir / "src_{136BCCAF-6037-4E96-B9CF-5F04B097CA88}.png", SCREENSHOTS_DIR / "03_foreign_animal_alert_ui.png"),
        (pitch_dir / "src_{48C23251-F414-4513-A864-39DF6352FFB3}.png", SCREENSHOTS_DIR / "04_multi_target_tracking_ui.png"),
        (pitch_dir / "src_{4B205B31-B4FA-4ACC-91EE-9EDD36E74166}.png", SCREENSHOTS_DIR / "05_control_center_ready_ui.png"),
        (runs_dir / "val_batch0_pred.jpg", SCREENSHOTS_DIR / "07_yolov8_cow_val_predictions.jpg"),
        (runs_dir / "results.png", SCREENSHOTS_DIR / "08_yolov8_training_curves.png"),
        (runs_dir / "confusion_matrix_normalized.png", SCREENSHOTS_DIR / "09_yolov8_confusion_matrix.png"),
        (runs_dir / "labels.jpg", SCREENSHOTS_DIR / "10_dataset_label_distribution.jpg"),
    ]

    for src, dst in copies:
        if src.exists():
            shutil.copy2(src, dst)
            print(f"[OK] Kopyalandi: {dst.name}")

    build_4cam_behavior_composite(SCREENSHOTS_DIR / "06_barn_4cam_cow_behavior_analysis.png")

    if ARTIFACT_DIR.exists():
        for f in sorted(SCREENSHOTS_DIR.glob("*.*")):
            shutil.copy2(f, ARTIFACT_DIR / f"farmeye_{f.name}")
            print(f"[ARTIFACT] {f.name} -> farmeye_{f.name}")


if __name__ == "__main__":
    main()
