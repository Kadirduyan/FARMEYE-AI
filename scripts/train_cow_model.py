"""
FARMEYE — YOLOv8 İnek Modeli Eğitimi (Fine-Tuning)
==================================================
C:/FarmEyeData/yolo_dataset/dataset.yaml veri seti (arkadan, yandan, onden 168 foto)
üzerinde yolov8n modelini fine-tune eder ve C:/FarmEye/FARMEYE-AI/models/yolov8n_cow.pt
ile yolov8n_cow.onnx olarak kaydeder.
"""
import shutil
from pathlib import Path
from ultralytics import YOLO

PROJECT = Path(r"C:\FarmEye\FARMEYE-AI")
DATASET_YAML = Path(r"C:\FarmEyeData\yolo_dataset\dataset.yaml")
MODELS_DIR = PROJECT / "models"

def main():
    model = YOLO(str(PROJECT / "yolov8n.pt"))
    results = model.train(
        data=str(DATASET_YAML),
        epochs=3,
        imgsz=416,
        batch=8,
        freeze=10,
        workers=0,
        project=str(MODELS_DIR / "runs"),
        name="farmeye_cow",
        exist_ok=True,
        verbose=True
    )
    best_pt = MODELS_DIR / "runs" / "farmeye_cow" / "weights" / "best.pt"
    if best_pt.exists():
        dst_pt = MODELS_DIR / "yolov8n_cow.pt"
        shutil.copy2(best_pt, dst_pt)
        print(f"Eğitilmiş model kaydedildi: {dst_pt}")
        # ONNX export
        trained = YOLO(str(dst_pt))
        onnx_path = trained.export(format="onnx", imgsz=640)
        print(f"ONNX export tamamlandı: {onnx_path}")

if __name__ == "__main__":
    main()
