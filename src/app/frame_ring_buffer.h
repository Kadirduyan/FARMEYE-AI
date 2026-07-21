#pragma once

#include <QImage>
#include <QMutex>
#include <QVector>
#include <cstdint>
#include <deque>
#include <vector>

namespace farmeye {

// BGR tabanlı ring: inference kopyasız okur, UI ihtiyaç olunca RGB'ye çevirir.
class FrameRingBuffer {
public:
    explicit FrameRingBuffer(int capacity = 60);

    void pushBgr(const uint8_t *bgr, int width, int height);
    bool tryLatestBgr(std::vector<uint8_t> &out, int &width, int &height) const;
    bool tryLatestQImage(QImage &out) const;

    // Alarm klipi için seyrek QImage geçmişi (düşük FPS)
    void pushClipFrame(const QImage &frame);
    QVector<QImage> lastSeconds(int seconds, int fpsHint = 8) const;

    void clear();
    int size() const;

private:
    struct BgrFrame {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> data;
    };

    int m_capacity;
    mutable QMutex m_mutex;
    BgrFrame m_latest;
    bool m_hasLatest = false;
    std::deque<QImage> m_clipFrames;
};

} // namespace farmeye
