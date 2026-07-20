#include "InferenceEngine.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include <opencv2/imgproc.hpp>

namespace farmeye {

namespace {

std::string resolveOnnxPath(const std::string &modelPath)
{
    namespace fs = std::filesystem;
    fs::path p(modelPath);
    if (p.extension() == ".onnx" && fs::exists(p)) {
        return p.string();
    }
    if (p.extension() == ".engine") {
        fs::path onnx = p;
        onnx.replace_extension(".onnx");
        if (fs::exists(onnx)) {
            std::cout << "[InferenceEngine] .engine verildi; OpenCV DNN icin "
                      << onnx.string() << " yukleniyor.\n"
                      << "  (TensorRT C++ SDK gelince .engine dogrudan kullanilacak)\n";
            return onnx.string();
        }
    }
    // models/yolov8n.onnx fallback yaninda
    fs::path sibling = p.parent_path() / "yolov8n.onnx";
    if (fs::exists(sibling)) {
        return sibling.string();
    }
    return {};
}

} // namespace

InferenceEngine::~InferenceEngine()
{
    unload();
}

bool InferenceEngine::loadEngine(const std::string &modelPath)
{
    unload();
    const std::string onnx = resolveOnnxPath(modelPath);
    if (onnx.empty()) {
        std::cerr << "[InferenceEngine] ONNX bulunamadi: " << modelPath << "\n";
        return false;
    }

    try {
        m_net = cv::dnn::readNetFromONNX(onnx);
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        m_loaded = true;
        std::cout << "[InferenceEngine] Yuklendi: " << onnx << "\n";
        return true;
    } catch (const cv::Exception &ex) {
        std::cerr << "[InferenceEngine] ONNX load hatasi: " << ex.what() << "\n";
        m_loaded = false;
        return false;
    }
}

void InferenceEngine::unload()
{
    m_net = cv::dnn::Net();
    m_loaded = false;
}

bool InferenceEngine::infer(const uint8_t *bgr, int width, int height, std::vector<Detection> &out)
{
    out.clear();
    if (!m_loaded || !bgr || width <= 0 || height <= 0) {
        return false;
    }
    cv::Mat img(height, width, CV_8UC3, const_cast<uint8_t *>(bgr));
    return infer(img, out);
}

bool InferenceEngine::infer(const cv::Mat &letterboxedBgr, std::vector<Detection> &out,
                            float confThresh, float nmsThresh)
{
    out.clear();
    if (!m_loaded || letterboxedBgr.empty()) {
        return false;
    }

    // blob: 1x3x640x640, RGB, /255
    cv::Mat blob;
    cv::dnn::blobFromImage(letterboxedBgr, blob, 1.0 / 255.0, cv::Size(m_inputSize, m_inputSize),
                           cv::Scalar(), true, false);
    m_net.setInput(blob);

    std::vector<cv::Mat> outputs;
    m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());
    if (outputs.empty()) {
        return false;
    }

    // Ultralytics YOLOv8 ONNX: [1, 84, 8400] → rows=84 (4+80), cols=8400
    cv::Mat pred = outputs[0];
    if (pred.dims == 3) {
        pred = pred.reshape(1, pred.size[1]); // 84 x 8400
    }

    const int rows = pred.rows; // 84
    const int cols = pred.cols; // 8400
    if (rows < 5 || cols < 1) {
        return false;
    }

    const int numClasses = rows - 4;
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (int i = 0; i < cols; ++i) {
        float bestScore = 0.f;
        int bestCls = -1;
        for (int c = 0; c < numClasses; ++c) {
            const float s = pred.at<float>(4 + c, i);
            if (s > bestScore) {
                bestScore = s;
                bestCls = c;
            }
        }
        if (bestScore < confThresh) {
            continue;
        }

        const float cx = pred.at<float>(0, i);
        const float cy = pred.at<float>(1, i);
        const float w = pred.at<float>(2, i);
        const float h = pred.at<float>(3, i);
        const int left = static_cast<int>(cx - w * 0.5f);
        const int top = static_cast<int>(cy - h * 0.5f);
        boxes.emplace_back(left, top, static_cast<int>(w), static_cast<int>(h));
        confidences.push_back(bestScore);
        classIds.push_back(bestCls);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThresh, nmsThresh, indices);

    out.reserve(indices.size());
    for (int idx : indices) {
        Detection d;
        d.x1 = static_cast<float>(boxes[idx].x);
        d.y1 = static_cast<float>(boxes[idx].y);
        d.x2 = static_cast<float>(boxes[idx].x + boxes[idx].width);
        d.y2 = static_cast<float>(boxes[idx].y + boxes[idx].height);
        d.score = confidences[idx];
        d.classId = classIds[idx];
        out.push_back(d);
    }
    return true;
}

} // namespace farmeye
