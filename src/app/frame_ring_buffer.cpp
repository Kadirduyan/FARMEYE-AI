#include "frame_ring_buffer.h"

#include <algorithm>

namespace farmeye {

FrameRingBuffer::FrameRingBuffer(int capacity)
    : m_capacity(capacity)
{
}

void FrameRingBuffer::push(const QImage &frame)
{
    QMutexLocker lock(&m_mutex);
    m_frames.push_back(frame.copy());
    while (static_cast<int>(m_frames.size()) > m_capacity) {
        m_frames.pop_front();
    }
}

QVector<QImage> FrameRingBuffer::lastSeconds(int seconds, int fpsHint) const
{
    QMutexLocker lock(&m_mutex);
    const int n = (std::max)(1, seconds * fpsHint);
    QVector<QImage> out;
    const int start = (std::max)(0, static_cast<int>(m_frames.size()) - n);
    for (int i = start; i < static_cast<int>(m_frames.size()); ++i) {
        out.push_back(m_frames[static_cast<size_t>(i)]);
    }
    return out;
}

void FrameRingBuffer::clear()
{
    QMutexLocker lock(&m_mutex);
    m_frames.clear();
}

int FrameRingBuffer::size() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_frames.size());
}

} // namespace farmeye
