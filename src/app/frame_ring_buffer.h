#pragma once

#include <QImage>
#include <QMutex>
#include <QVector>
#include <deque>

namespace farmeye {

class FrameRingBuffer {
public:
    explicit FrameRingBuffer(int capacity = 150);

    void push(const QImage &frame);
    QVector<QImage> lastSeconds(int seconds, int fpsHint = 15) const;
    void clear();
    int size() const;

private:
    int m_capacity;
    mutable QMutex m_mutex;
    std::deque<QImage> m_frames;
};

} // namespace farmeye
