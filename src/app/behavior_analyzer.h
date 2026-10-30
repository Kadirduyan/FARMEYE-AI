#pragma once

#include "engine/class_policy.h"
#include "app/farmeye_store.h"
#include "tracking/tracker.h"

#include <QRectF>
#include <QString>
#include <map>
#include <vector>

namespace farmeye {

// Canlı inek track'lerinden davranış çıkarır → FarmEyeStore.
// YOLO sınıfı değil; bbox oranları + ROI süreleri + hız düzensizliği.
struct BehaviorRoi {
    QRectF norm; // 0..1 frame oranları
    QString name;
};

class BehaviorAnalyzer {
public:
    explicit BehaviorAnalyzer(FarmEyeStore *store);

    // Suluk / yemlik bölgeleri (normalize 0-1). Boşsa varsayılan sol/sağ alt kullanılır.
    void setWaterRoi(const QRectF &normRect);
    void setFeedRoi(const QRectF &normRect);

    // Her inference tick'inde çağır (cow track'leri).
    void update(int cameraId, int frameW, int frameH, const std::vector<Track> &tracks, double dtSec);

    void reset();

private:
    struct CowState {
        int animalId = 0;
        float prevCx = 0, prevCy = 0;
        bool hasPrev = false;
        std::vector<float> speedHist; // son N hız
        bool lying = false;
        bool inWater = false;
        bool inFeed = false;
        double waterAcc = 0;
        double feedAcc = 0;
        double lieAcc = 0;
        double limpCooldown = 0;
        bool limpActive = false;
    };

    int ensureAnimalForTrack(int trackId);
    static bool isLying(float x1, float y1, float x2, float y2);
    static bool insideRoi(float cx, float cy, float frameW, float frameH, const QRectF &norm);
    void flushDurations(CowState &st, int cameraId);

    FarmEyeStore *m_store = nullptr;
    QRectF m_waterRoi{0.05, 0.55, 0.30, 0.40}; // sol alt
    QRectF m_feedRoi{0.65, 0.55, 0.30, 0.40};  // sağ alt
    std::map<int, CowState> m_byTrack;         // tracker id → state
    std::map<int, int> m_trackToAnimal;        // tracker id → animals.id
};

} // namespace farmeye
