"""Build TensorRT FP16 engine from yolov8n.onnx (GTX 1650 / CUDA 12).

Usage:
  python scripts/build_engine.py
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ONNX = ROOT / "models" / "yolov8n.onnx"
ENGINE = ROOT / "models" / "yolov8n_fp16.engine"


def main() -> int:
    if not ONNX.is_file():
        print(f"[HATA] ONNX yok: {ONNX}")
        return 1

    try:
        import tensorrt as trt
    except ImportError as exc:
        print("[HATA] tensorrt import edilemedi:", exc)
        print("  pip install tensorrt nvidia-cudnn-cu12")
        return 1

    logger = trt.Logger(trt.Logger.INFO)
    builder = trt.Builder(logger)
    network_flags = 1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH)
    network = builder.create_network(network_flags)
    parser = trt.OnnxParser(network, logger)

    print(f"Parsing ONNX: {ONNX}")
    with ONNX.open("rb") as f:
        if not parser.parse(f.read()):
            for i in range(parser.num_errors):
                print(parser.get_error(i))
            return 1

    config = builder.create_builder_config()
    # ~1 GB workspace — GTX 1650 4 GB VRAM için güvenli
    config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 1 << 30)
    if builder.platform_has_fast_fp16:
        config.set_flag(trt.BuilderFlag.FP16)
        print("FP16 enabled")
    else:
        print("FP16 not available, building FP32")

    print("Building engine (birkaç dakika sürebilir)...")
    serialized = builder.build_serialized_network(network, config)
    if serialized is None:
        print("[HATA] Engine build başarısız")
        return 1

    ENGINE.parent.mkdir(parents=True, exist_ok=True)
    ENGINE.write_bytes(bytes(serialized))
    print(f"OK: {ENGINE} ({ENGINE.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
