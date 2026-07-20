#include "tracker.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace farmeye {

Tracker::Tracker(float iouThreshold, int maxLost)
    : m_iouThreshold(iouThreshold)
    , m_maxLost(maxLost)
{
}

float Tracker::iou(const Track &t, const ClassifiedDetection &d)
{
    const float xx1 = (std::max)(t.x1, d.x1);
    const float yy1 = (std::max)(t.y1, d.y1);
    const float xx2 = (std::min)(t.x2, d.x2);
    const float yy2 = (std::min)(t.y2, d.y2);
    const float w = (std::max)(0.f, xx2 - xx1);
    const float h = (std::max)(0.f, yy2 - yy1);
    const float inter = w * h;
    const float a1 = (std::max)(0.f, t.x2 - t.x1) * (std::max)(0.f, t.y2 - t.y1);
    const float a2 = (std::max)(0.f, d.x2 - d.x1) * (std::max)(0.f, d.y2 - d.y1);
    const float uni = a1 + a2 - inter;
    return uni > 0.f ? inter / uni : 0.f;
}

std::vector<Track> Tracker::update(const std::vector<ClassifiedDetection> &dets)
{
    for (Track &t : m_tracks) {
        t.timeSinceUpdate += 1;
    }

    std::vector<bool> detUsed(dets.size(), false);
    std::vector<bool> trackUsed(m_tracks.size(), false);

    // Greedy IoU matching (aynı kind tercih)
    struct Pair {
        int ti;
        int di;
        float score;
    };
    std::vector<Pair> pairs;
    for (size_t ti = 0; ti < m_tracks.size(); ++ti) {
        for (size_t di = 0; di < dets.size(); ++di) {
            if (m_tracks[ti].kind != dets[di].kind) {
                continue;
            }
            const float s = iou(m_tracks[ti], dets[di]);
            if (s >= m_iouThreshold) {
                pairs.push_back({static_cast<int>(ti), static_cast<int>(di), s});
            }
        }
    }
    std::sort(pairs.begin(), pairs.end(),
              [](const Pair &a, const Pair &b) { return a.score > b.score; });

    for (const Pair &p : pairs) {
        if (trackUsed[p.ti] || detUsed[p.di]) {
            continue;
        }
        Track &t = m_tracks[p.ti];
        const ClassifiedDetection &d = dets[p.di];
        t.x1 = d.x1;
        t.y1 = d.y1;
        t.x2 = d.x2;
        t.y2 = d.y2;
        t.labelTr = d.labelTr;
        t.timeSinceUpdate = 0;
        t.age += 1;
        trackUsed[p.ti] = true;
        detUsed[p.di] = true;
    }

    // Yeni track'ler
    for (size_t di = 0; di < dets.size(); ++di) {
        if (detUsed[di]) {
            continue;
        }
        Track t;
        t.id = m_nextId++;
        t.x1 = dets[di].x1;
        t.y1 = dets[di].y1;
        t.x2 = dets[di].x2;
        t.y2 = dets[di].y2;
        t.kind = dets[di].kind;
        t.labelTr = dets[di].labelTr;
        t.age = 1;
        t.timeSinceUpdate = 0;
        m_tracks.push_back(t);
    }

    // Kayıp track'leri sil
    m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(),
                                  [&](const Track &t) { return t.timeSinceUpdate > m_maxLost; }),
                   m_tracks.end());

    std::vector<Track> active;
    for (const Track &t : m_tracks) {
        if (t.timeSinceUpdate == 0) {
            active.push_back(t);
        }
    }
    return active;
}

} // namespace farmeye
