#pragma once

#include <opencv2/core.hpp>

namespace farmeye {

struct LetterboxMeta {
    float scale = 1.f;
    int padX = 0;
    int padY = 0;
    int origW = 0;
    int origH = 0;
    int inputSize = 640;
};

// BGR kareyi 640x640 letterbox yapar (YOLO girişi).
cv::Mat letterbox(const cv::Mat &bgr, LetterboxMeta &meta, int size = 640);

// 640 uzayındaki kutuyu orijinal kare koordinatına çevirir.
void mapBoxToOriginal(float &x1, float &y1, float &x2, float &y2, const LetterboxMeta &meta);

} // namespace farmeye
