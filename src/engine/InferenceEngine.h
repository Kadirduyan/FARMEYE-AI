#pragma once

// InferenceEngine — YOLOv8 nano tespit.
// Not: TensorRT C++ SDK header'ları bu PC'de yok; OpenCV DNN + yolov8n.onnx kullanılır.
// models/yolov8n_fp16.engine TensorRT ile üretildi (Python); C++ SDK gelince aynı API'ye bağlanır.

#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

namespace farmeye {

struct Detection {
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    float score = 0;
    int classId = -1;
};

class InferenceEngine {
public:
    InferenceEngine() = default;
    ~InferenceEngine();

    // .onnx yolu (önerilen). .engine verilirse aynı klasördeki .onnx denenir.
    bool loadEngine(const std::string &modelPath);
    void unload();

    // letterbox edilmiş 640 BGR kare (CV_8UC3)
    bool infer(const cv::Mat &letterboxedBgr, std::vector<Detection> &out,
               float confThresh = 0.25f, float nmsThresh = 0.45f);

    // Ham BGR (eski API uyumu)
    bool infer(const uint8_t *bgr, int width, int height, std::vector<Detection> &out);

    bool isLoaded() const { return m_loaded; }

private:
    bool m_loaded = false;
    cv::dnn::Net m_net;
    int m_inputSize = 640; // ONNX sabit 640 — 416 çökertiyor
};

} // namespace farmeye
