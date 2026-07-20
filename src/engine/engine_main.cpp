// farmeye_engine — konsol motoru (UI yok)
// Akış: GStreamer → letterbox 640 → YOLO ONNX → class policy → tracker → konsol

#include "engine/GstCameraCapture.h"
#include "engine/InferenceEngine.h"
#include "engine/class_policy.h"
#include "engine/preprocess.h"
#include "tracking/tracker.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <string>

namespace {

std::string idFmt(int id)
{
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << id;
    return oss.str();
}

std::string defaultModelPath()
{
#if defined(FARMEYE_SOURCE_DIR)
    return std::string(FARMEYE_SOURCE_DIR) + "/models/yolov8n_fp16.engine";
#else
    return "models/yolov8n_fp16.engine";
#endif
}

void printUsage(const char *argv0)
{
    std::cout
        << "FarmEye Engine (Aşama 1)\n"
        << "Kullanim:\n"
        << "  " << argv0 << " webcam\n"
        << "  " << argv0 << " webcam:0\n"
        << "  " << argv0 << " C:/path/video.mp4\n"
        << "  " << argv0 << " rtsp://ip/stream\n"
        << "Opsiyonel 2. arguman: model (.engine veya .onnx)\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string source = argv[1];
    const std::string modelPath = (argc >= 3) ? argv[2] : defaultModelPath();

    std::cout << "=== FarmEye Engine ===\n"
              << "Kaynak: " << source << "\n"
              << "Model : " << modelPath << "\n";

    farmeye::InferenceEngine engine;
    if (!engine.loadEngine(modelPath)) {
        std::cerr << "Model yuklenemedi.\n";
        return 1;
    }

    farmeye::GstCameraCapture cap;
    if (!cap.open(source)) {
        std::cerr << "Kaynak acilamadi: " << source << "\n";
        return 1;
    }

    farmeye::Tracker tracker;
    farmeye::FrameBuffer frame;
    bool savedDebug = false;
    int frameIndex = 0;
    int framesWithTracks = 0;
    const auto t0 = std::chrono::steady_clock::now();

    std::cout << "Calisiyor... (Ctrl+C ile durdur)\n";

    while (cap.read(frame)) {
        ++frameIndex;
        cv::Mat bgr(frame.height, frame.width, CV_8UC3, frame.data.data());

        farmeye::LetterboxMeta meta;
        cv::Mat input = farmeye::letterbox(bgr, meta, 640);

        if (!savedDebug) {
#if defined(FARMEYE_SOURCE_DIR)
            const std::string dbg = std::string(FARMEYE_SOURCE_DIR) + "/models/debug_640.jpg";
#else
            const std::string dbg = "models/debug_640.jpg";
#endif
            cv::imwrite(dbg, input);
            std::cout << "[debug] letterbox kaydedildi: " << dbg << "\n";
            savedDebug = true;
        }

        std::vector<farmeye::Detection> dets;
        if (!engine.infer(input, dets)) {
            continue;
        }

        // Kutuları orijinal koordinata map et
        for (auto &d : dets) {
            farmeye::mapBoxToOriginal(d.x1, d.y1, d.x2, d.y2, meta);
        }

        auto classified = farmeye::applyClassPolicy(dets);
        auto tracks = tracker.update(classified);

        if (!tracks.empty()) {
            ++framesWithTracks;
            std::cout << "--- frame " << frameIndex << " ---\n";
            for (const auto &t : tracks) {
                const int cx = static_cast<int>((t.x1 + t.x2) * 0.5f);
                const int cy = static_cast<int>((t.y1 + t.y2) * 0.5f);
                std::cout << "ID: " << idFmt(t.id) << " - Konum: " << cx << "," << cy
                          << " - Tip: " << t.labelTr << "\n";
                if (t.kind == farmeye::ObjectKind::Human) {
                    std::cout << "UYARI: insan (ID: " << idFmt(t.id) << ")\n";
                } else if (t.kind == farmeye::ObjectKind::OtherAnimal) {
                    std::cout << "UYARI: baska canli (ID: " << idFmt(t.id) << ")\n";
                }
            }
        } else if (frameIndex % 30 == 0) {
            const auto t1 = std::chrono::steady_clock::now();
            const double sec =
                std::chrono::duration<double>(t1 - t0).count();
            const double fps = sec > 0 ? frameIndex / sec : 0;
            std::cout << "[info] frame=" << frameIndex << " fps~" << std::fixed
                      << std::setprecision(1) << fps << " (tespit yok)\n";
        }

        // Video dosyasında sonsuz döngü olmasın diye: 0 okuma = bitti (read false)
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "Bitti. frame=" << frameIndex << " trackli_frame=" << framesWithTracks
              << " sure=" << sec << "s\n";
    return 0;
}
