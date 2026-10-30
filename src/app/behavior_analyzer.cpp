#include "behavior_analyzer.h"

#include <QDateTime>
#include <cmath>

namespace farmeye {

BehaviorAnalyzer::BehaviorAnalyzer(FarmEyeStore *store)
    : m_store(store)
{
}

void BehaviorAnalyzer::setWaterRoi(const QRectF &normRect)
{
    m_waterRoi = normRect;
}

void BehaviorAnalyzer::setFeedRoi(const QRectF &normRect)
{
    m_feedRoi = normRect;
}

void BehaviorAnalyzer::reset()
{
    m_byTrack.clear();
    m_trackToAnimal.clear();
}

bool BehaviorAnalyzer::isLying(float x1, float y1, float x2, float y2)
{
    const float w = (std::max)(1.f, x2 - x1);
    const float h = (std::max)(1.f, y2 - y1);
    // Yatınca kutu genelde daha yatay (w/h yüksek)
    return (w / h) >= 1.55f && h / w <= 0.85f;
}

bool BehaviorAnalyzer::insideRoi(float cx, float cy, float frameW, float frameH, const QRectF &norm)
{
    if (frameW <= 1 || frameH <= 1 || norm.isEmpty()) {
        return false;
    }
    const float nx = cx / frameW;
    const float ny = cy / frameH;
    return nx >= norm.left() && nx <= norm.right() && ny >= norm.top() && ny <= norm.bottom();
}

int BehaviorAnalyzer::ensureAnimalForTrack(int trackId)
{
    auto it = m_trackToAnimal.find(trackId);
    if (it != m_trackToAnimal.end()) {
        return it->second;
    }
    if (!m_store || !m_store->isOpen()) {
        return 0;
    }
    const QString tag = QStringLiteral("INEK-%1").arg(trackId, 3, 10, QChar('0'));
    const int id = m_store->ensureAnimal(tag);
    if (id > 0) {
        m_trackToAnimal[trackId] = id;
    }
    return id;
}

void BehaviorAnalyzer::flushDurations(CowState &st, int cameraId)
{
    if (!m_store || st.animalId <= 0) {
        return;
    }
    const QDate day = QDate::currentDate();
    if (st.waterAcc >= 1.0) {
        m_store->applyDurationToDaily(st.animalId, BehaviorEventType::DrinkEnd,
                                      static_cast<int>(st.waterAcc), day);
        st.waterAcc = 0;
    }
    if (st.feedAcc >= 1.0) {
        m_store->applyDurationToDaily(st.animalId, BehaviorEventType::EatEnd,
                                      static_cast<int>(st.feedAcc), day);
        st.feedAcc = 0;
    }
    if (st.lieAcc >= 1.0) {
        m_store->applyDurationToDaily(st.animalId, BehaviorEventType::LieEnd,
                                      static_cast<int>(st.lieAcc), day);
        st.lieAcc = 0;
    }
    Q_UNUSED(cameraId);
}

void BehaviorAnalyzer::update(int cameraId, int frameW, int frameH, const std::vector<Track> &tracks,
                              double dtSec)
{
    if (!m_store || !m_store->isOpen() || dtSec <= 0) {
        return;
    }
    const double dt = (std::min)(dtSec, 2.0);
    const QDateTime now = QDateTime::currentDateTime();

    std::map<int, bool> seen;
    for (const Track &t : tracks) {
        if (t.kind != ObjectKind::Cow) {
            continue;
        }
        seen[t.id] = true;
        CowState &st = m_byTrack[t.id];
        if (st.animalId <= 0) {
            st.animalId = ensureAnimalForTrack(t.id);
        }
        if (st.animalId <= 0) {
            continue;
        }

        const float cx = 0.5f * (t.x1 + t.x2);
        const float cy = 0.5f * (t.y1 + t.y2);

        // --- Yatma ---
        const bool lyingNow = isLying(t.x1, t.y1, t.x2, t.y2);
        if (lyingNow && !st.lying) {
            st.lying = true;
            m_store->recordEvent(st.animalId, BehaviorEventType::LieStart, now, cameraId, 0.7,
                                 QStringLiteral("Yatma algılandı"));
        } else if (!lyingNow && st.lying) {
            st.lying = false;
            m_store->recordEvent(st.animalId, BehaviorEventType::LieEnd, now, cameraId, 0.7,
                                 QStringLiteral("Ayağa kalktı"));
            flushDurations(st, cameraId);
        }
        if (st.lying) {
            st.lieAcc += dt;
        }

        // --- Su / yem ROI ---
        const bool waterNow = insideRoi(cx, cy, static_cast<float>(frameW), static_cast<float>(frameH),
                                        m_waterRoi);
        const bool feedNow = insideRoi(cx, cy, static_cast<float>(frameW), static_cast<float>(frameH),
                                       m_feedRoi);

        if (waterNow && !st.inWater) {
            st.inWater = true;
            m_store->recordEvent(st.animalId, BehaviorEventType::DrinkStart, now, cameraId, 0.6,
                                 QStringLiteral("Suluk bölgesi"));
        } else if (!waterNow && st.inWater) {
            st.inWater = false;
            m_store->recordEvent(st.animalId, BehaviorEventType::DrinkEnd, now, cameraId, 0.6,
                                 QStringLiteral("Suluk bölgesinden çıktı"));
            flushDurations(st, cameraId);
        }
        if (st.inWater) {
            st.waterAcc += dt;
        }

        if (feedNow && !st.inFeed) {
            st.inFeed = true;
            m_store->recordEvent(st.animalId, BehaviorEventType::EatStart, now, cameraId, 0.6,
                                 QStringLiteral("Yemlik bölgesi"));
        } else if (!feedNow && st.inFeed) {
            st.inFeed = false;
            m_store->recordEvent(st.animalId, BehaviorEventType::EatEnd, now, cameraId, 0.6,
                                 QStringLiteral("Yemlik bölgesinden çıktı"));
            flushDurations(st, cameraId);
        }
        if (st.inFeed) {
            st.feedAcc += dt;
        }

        // --- Topallık PoC: hız salınımı ---
        if (st.hasPrev && !st.lying) {
            const float dx = cx - st.prevCx;
            const float dy = cy - st.prevCy;
            const float speed = std::sqrt(dx * dx + dy * dy) / static_cast<float>(dt);
            st.speedHist.push_back(speed);
            if (st.speedHist.size() > 12) {
                st.speedHist.erase(st.speedHist.begin());
            }
            if (st.speedHist.size() >= 8) {
                double mean = 0;
                for (float s : st.speedHist) {
                    mean += s;
                }
                mean /= st.speedHist.size();
                double var = 0;
                for (float s : st.speedHist) {
                    const double d = s - mean;
                    var += d * d;
                }
                var /= st.speedHist.size();
                const double cv = mean > 1.0 ? std::sqrt(var) / mean : 0.0;
                // Yüksek katsayı + yürüyor → şüpheli topallık
                if (mean > 8.0 && cv > 0.85) {
                    st.limpCooldown = cv;
                    if (!st.limpActive) {
                        st.limpActive = true;
                        m_store->recordEvent(st.animalId, BehaviorEventType::LimpStart, now, cameraId,
                                             cv, QStringLiteral("Topallık şüphesi (hız düzensiz)"));
                    }
                } else if (st.limpActive && cv < 0.45) {
                    st.limpActive = false;
                    m_store->recordEvent(st.animalId, BehaviorEventType::LimpEnd, now, cameraId, cv,
                                         QStringLiteral("Topallık şüphesi geçti"));
                }
            }
        }
        st.prevCx = cx;
        st.prevCy = cy;
        st.hasPrev = true;
    }

    // Kaybolan track'ler — süreleri yaz
    for (auto it = m_byTrack.begin(); it != m_byTrack.end();) {
        if (!seen.count(it->first)) {
            flushDurations(it->second, cameraId);
            m_trackToAnimal.erase(it->first);
            it = m_byTrack.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace farmeye
