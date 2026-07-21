#include "frame_ring_buffer.h"

#include <QMutexLocker>
#include <algorithm>
#include <cstring>
#include <opencv2/imgproc.hpp>

namespace farmeye {

FrameRingBuffer::FrameRingBuffer(int capacity)
    : m_capacity((std::max)(8, capacity))
{
}

void FrameRingBuffer::pushBgr(const uint8_t *bgr, int width, int height)
{
    if (!bgr || width <= 0 || height <= 0) {
        return;
    }
    const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height) * 3u;
    QMutexLocker lock(&m_mutex);
    m_latest.width = width;
    m_latest.height = height;
    m_latest.data.resize(n);
    std::memcpy(m_latest.data.data(), bgr, n);
    m_hasLatest = true;
}

bool FrameRingBuffer::tryLatestBgr(std::vector<uint8_t> &out, int &width, int &height) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_hasLatest || m_latest.data.empty()) {
        return false;
    }
    width = m_latest.width;
    height = m_latest.height;
    out = m_latest.data;
    return true;
}

bool FrameRingBuffer::tryLatestQImage(QImage &out) const
{
    std::vector<uint8_t> bgr;
    int w = 0;
    int h = 0;
    if (!tryLatestBgr(bgr, w, h)) {
        out = QImage();
        return false;
    }
    cv::Mat bgrMat(h, w, CV_8UC3, bgr.data());
    // UI için gerekirse küçült (büyük karelerde CPU tasarrufu)
    cv::Mat small = bgrMat;
    if (w > 640) {
        const double scale = 640.0 / w;
        cv::resize(bgrMat, small, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    cv::Mat rgb;
    cv::cvtColor(small, rgb, cv::COLOR_BGR2RGB);
    out = QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    return true;
}

void FrameRingBuffer::pushClipFrame(const QImage &frame)
{
    if (frame.isNull()) {
        return;
    }
    // Klip için küçük tut (CPU/RAM)
    const QImage small = frame.scaled(320, 180, Qt::KeepAspectRatio, Qt::FastTransformation);
    QMutexLocker lock(&m_mutex);
    m_clipFrames.push_back(small);
    while (static_cast<int>(m_clipFrames.size()) > m_capacity) {
        m_clipFrames.pop_front();
    }
}

QVector<QImage> FrameRingBuffer::lastSeconds(int seconds, int fpsHint) const
{
    QMutexLocker lock(&m_mutex);
    const int n = (std::max)(1, seconds * fpsHint);
    QVector<QImage> out;
    const int start = (std::max)(0, static_cast<int>(m_clipFrames.size()) - n);
    for (int i = start; i < static_cast<int>(m_clipFrames.size()); ++i) {
        out.push_back(m_clipFrames[static_cast<size_t>(i)]);
    }
    return out;
}

void FrameRingBuffer::clear()
{
    QMutexLocker lock(&m_mutex);
    m_latest = {};
    m_hasLatest = false;
    m_clipFrames.clear();
}

int FrameRingBuffer::size() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_clipFrames.size());
}

} // namespace farmeye
