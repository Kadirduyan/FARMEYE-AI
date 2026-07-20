#include "preprocess.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

namespace farmeye {

cv::Mat letterbox(const cv::Mat &bgr, LetterboxMeta &meta, int size)
{
    meta.origW = bgr.cols;
    meta.origH = bgr.rows;
    meta.inputSize = size;

    const float r = std::min(static_cast<float>(size) / bgr.cols,
                             static_cast<float>(size) / bgr.rows);
    meta.scale = r;

    const int newW = static_cast<int>(std::round(bgr.cols * r));
    const int newH = static_cast<int>(std::round(bgr.rows * r));
    meta.padX = (size - newW) / 2;
    meta.padY = (size - newH) / 2;

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

    cv::Mat out(size, size, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(out(cv::Rect(meta.padX, meta.padY, newW, newH)));
    return out;
}

void mapBoxToOriginal(float &x1, float &y1, float &x2, float &y2, const LetterboxMeta &meta)
{
    x1 = (x1 - meta.padX) / meta.scale;
    y1 = (y1 - meta.padY) / meta.scale;
    x2 = (x2 - meta.padX) / meta.scale;
    y2 = (y2 - meta.padY) / meta.scale;

    x1 = std::clamp(x1, 0.f, static_cast<float>(meta.origW - 1));
    y1 = std::clamp(y1, 0.f, static_cast<float>(meta.origH - 1));
    x2 = std::clamp(x2, 0.f, static_cast<float>(meta.origW - 1));
    y2 = std::clamp(y2, 0.f, static_cast<float>(meta.origH - 1));
}

} // namespace farmeye
