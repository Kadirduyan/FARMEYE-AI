#pragma once

// ByteTrack-benzeri basit tracker: IoU eşleme + sabit ID.
// Tam ByteTrack Kalman ileride third_party ile değiştirilebilir; API aynı kalır.

#include "engine/class_policy.h"

#include <cstdint>
#include <string>
#include <vector>

namespace farmeye {

struct Track {
    int id = 0; // 1-based; konsolda 001 formatında basılır
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    ObjectKind kind = ObjectKind::Ignore;
    std::string labelTr;
    int age = 0;
    int timeSinceUpdate = 0;
};

class Tracker {
public:
    explicit Tracker(float iouThreshold = 0.3f, int maxLost = 30);

    // ClassifiedDetection listesini güncelle → aktif track'ler
    std::vector<Track> update(const std::vector<ClassifiedDetection> &dets);

private:
    float m_iouThreshold;
    int m_maxLost;
    int m_nextId = 1;
    std::vector<Track> m_tracks;

    static float iou(const Track &t, const ClassifiedDetection &d);
};

} // namespace farmeye
