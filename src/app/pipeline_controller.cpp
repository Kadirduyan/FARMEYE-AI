#include "pipeline_controller.h"

#include "app/behavior_analyzer.h"
#include "app/person_identity_store.h"
#include "engine/GstCameraCapture.h"
#include "engine/InferenceEngine.h"
#include "engine/class_policy.h"
#include "engine/preprocess.h"

#include <QDateTime>
#include <QMetaType>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>
#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <opencv2/imgproc.hpp>
#include <set>

namespace farmeye {

namespace {

AlertLevel levelForKind(ObjectKind k)
{
    switch (k) {
    case ObjectKind::Human:
        return AlertLevel::Suspicious;
    case ObjectKind::OtherAnimal:
        return AlertLevel::Abnormal;
    case ObjectKind::Cow:
    default:
        return AlertLevel::Normal;
    }
}

QImage bgrToQImage(const uint8_t *bgr, int w, int h)
{
    if (!bgr || w <= 0 || h <= 0) {
        return {};
    }
    cv::Mat bgrMat(h, w, CV_8UC3, const_cast<uint8_t *>(bgr));
    cv::Mat rgb;
    cv::cvtColor(bgrMat, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
}

} // namespace

// ---------------------------------------------------------------------------
// Capture: GStreamer → BGR ring (sinyal yok → UI flood yok)
// ---------------------------------------------------------------------------
class CaptureWorker : public QObject {
    Q_OBJECT
public:
    CaptureWorker(int camId, FrameRingBuffer *ring, const QString &uri)
        : m_camId(camId)
        , m_ring(ring)
        , m_uri(uri)
    {
    }

public slots:
    void setUri(const QString &uri)
    {
        {
            std::lock_guard<std::mutex> lock(m_uriMutex);
            m_uri = uri;
        }
        m_restart = true;
    }

    void startLoop()
    {
        if (m_busy.exchange(true)) {
            return;
        }
        m_stop = false;
        run();
        m_busy = false;
    }

    void stopLoop() { m_stop = true; }

signals:
    void statusMessage(const QString &msg);
    void failed(const QString &reason);

private:
    void run()
    {
        GstCameraCapture cap;
        QElapsedTimer clipTimer;
        clipTimer.start();

        // 4 kamera hedefi: 640x360 @ 12 fps (GStreamer videorate)
        constexpr int kW = 640;
        constexpr int kH = 360;
        constexpr int kFps = 12;

        while (!m_stop) {
            QString uriLocal;
            {
                std::lock_guard<std::mutex> lock(m_uriMutex);
                uriLocal = m_uri;
            }

            if (uriLocal.isEmpty()) {
                QThread::msleep(200);
                continue;
            }

            if (m_restart || !cap.isOpen()) {
                cap.close();
                if (!cap.open(uriLocal.toStdString(), kW, kH, kFps)) {
                    emit statusMessage(QStringLiteral("Kamera açılamadı (cam%1): ").arg(m_camId + 1) + uriLocal);
                    QThread::msleep(1000);
                    continue;
                }
                m_restart = false;
                m_gotFrame = false;
                emit statusMessage(QStringLiteral("Kamera açılıyor (cam%1)...").arg(m_camId + 1));
            }

            FrameBuffer fb;
            if (!cap.read(fb)) {
                continue;
            }

            if (!m_gotFrame) {
                m_gotFrame = true;
                emit statusMessage(QStringLiteral("Görüntü geldi (cam%1) %2x%3")
                                       .arg(m_camId + 1)
                                       .arg(fb.width)
                                       .arg(fb.height));
            }

            if (m_ring) {
                m_ring->pushBgr(fb.data.data(), fb.width, fb.height);
                // Alarm klipi: ~2 fps
                if (clipTimer.elapsed() >= 500) {
                    clipTimer.restart();
                    m_ring->pushClipFrame(bgrToQImage(fb.data.data(), fb.width, fb.height));
                }
            }
        }

        cap.close();
    }

    int m_camId = 0;
    FrameRingBuffer *m_ring = nullptr;
    QString m_uri;
    std::mutex m_uriMutex;

    std::atomic<bool> m_stop{true};
    std::atomic<bool> m_restart{false};
    std::atomic<bool> m_busy{false};
    bool m_gotFrame = false;
};

// ---------------------------------------------------------------------------
// Inference: sadece aktif kamera, BGR ring'den, düşük FPS
// ---------------------------------------------------------------------------
class InferenceWorker : public QObject {
    Q_OBJECT
public:
    explicit InferenceWorker(const std::array<FrameRingBuffer *, PipelineController::kMaxCameras> &rings,
                             std::atomic<int> *activeId, PersonIdentityStore *people,
                             std::atomic<int> *bindNameId, FarmEyeStore *farm)
        : m_rings(rings)
        , m_activeId(activeId)
        , m_people(people)
        , m_bindNameId(bindNameId)
        , m_farm(farm)
        , m_behavior(farm)
    {
    }

public slots:
    void setModelPath(const QString &path)
    {
        {
            std::lock_guard<std::mutex> lock(m_modelMutex);
            m_modelPath = path;
        }
        m_restartModel = true;
    }

    void startLoop()
    {
        if (m_busy.exchange(true)) {
            return;
        }
        m_stop = false;
        run();
        m_busy = false;
    }

    void stopLoop() { m_stop = true; }

signals:
    void frameReady(const QImage &frame, const QVector<farmeye::TrackOverlay> &tracks);
    void alertRaised(farmeye::AlertEvent alert);
    void statusMessage(const QString &msg);
    void failed(const QString &reason);

private:
    QString resolveModelPath()
    {
        QString model;
        {
            std::lock_guard<std::mutex> lock(m_modelMutex);
            model = m_modelPath;
        }
        if (model.isEmpty()) {
#if defined(FARMEYE_SOURCE_DIR)
            model = QStringLiteral(FARMEYE_SOURCE_DIR) + QStringLiteral("/models/yolov8n_fp16.engine");
#else
            model = QStringLiteral("models/yolov8n_fp16.engine");
#endif
        }
        return model;
    }

    void run()
    {
        InferenceEngine engine;
        QString model = resolveModelPath();
        if (!engine.loadEngine(model.toStdString())) {
            emit statusMessage(QStringLiteral("Model yüklenemedi"));
            emit failed(QStringLiteral("Model yüklenemedi"));
            return;
        }

        Tracker tracker(0.25f, 60); // daha uzun takip — flicker'da yeni ID açmasın
        std::map<int, int> sessionToPersistent;
        std::map<int, std::array<float, 4>> sessionBoxes; // trackId → box
        struct GhostSlot {
            int pid = 0;
            float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            int ttl = 0;
        };
        std::vector<GhostSlot> ghosts;
        std::set<int> alertedKeys;
        std::map<int, int> framesSeen;
        int lastCam = -1;

        constexpr int kInferFps = 2; // 640 YOLO pahalı — 2 fps takip için yeterli
        constexpr int kInput = 640; // model sabit 640
        const qint64 intervalMs = 1000 / kInferFps;
        QElapsedTimer throttle;

        auto boxIou = [](float ax1, float ay1, float ax2, float ay2, float bx1, float by1, float bx2,
                         float by2) {
            const float xx1 = (std::max)(ax1, bx1);
            const float yy1 = (std::max)(ay1, by1);
            const float xx2 = (std::min)(ax2, bx2);
            const float yy2 = (std::min)(ay2, by2);
            const float w = (std::max)(0.f, xx2 - xx1);
            const float h = (std::max)(0.f, yy2 - yy1);
            const float inter = w * h;
            const float a = (std::max)(0.f, ax2 - ax1) * (std::max)(0.f, ay2 - ay1);
            const float b = (std::max)(0.f, bx2 - bx1) * (std::max)(0.f, by2 - by1);
            const float uni = a + b - inter;
            return uni > 0.f ? inter / uni : 0.f;
        };

        while (!m_stop) {
            const int camId = m_activeId->load();
            if (camId < 0 || camId >= PipelineController::kMaxCameras) {
                QThread::msleep(20);
                continue;
            }

            FrameRingBuffer *ring = m_rings[camId];
            if (!ring) {
                QThread::msleep(20);
                continue;
            }

            if (m_restartModel) {
                m_restartModel = false;
                model = resolveModelPath();
                if (!engine.loadEngine(model.toStdString())) {
                    emit statusMessage(QStringLiteral("Model yüklenemedi"));
                    continue;
                }
            }

            if (camId != lastCam) {
                lastCam = camId;
                tracker = Tracker(0.25f, 60);
                sessionToPersistent.clear();
                sessionBoxes.clear();
                ghosts.clear();
                alertedKeys.clear();
                framesSeen.clear();
                m_behavior.reset();
            }

            std::vector<uint8_t> bgrData;
            int w = 0;
            int h = 0;
            if (!ring->tryLatestBgr(bgrData, w, h)) {
                QThread::msleep(20);
                continue;
            }

            throttle.restart();

            cv::Mat bgr(h, w, CV_8UC3, bgrData.data());
            LetterboxMeta meta;
            cv::Mat input = letterbox(bgr, meta, kInput);

            std::vector<Detection> dets;
            // Telefon ekranındaki inek fotoğrafları için daha düşük eşik
            engine.infer(input, dets, 0.18f, 0.45f);
            for (auto &d : dets) {
                mapBoxToOriginal(d.x1, d.y1, d.x2, d.y2, meta);
            }
            auto classified = suppressNestedHumans(applyClassPolicy(dets));
            auto tracks = tracker.update(classified);

            // İnek davranış: su/yem ROI, yatma, topallık şüphesi → FarmEyeStore
            const double dtSec = intervalMs / 1000.0;
            m_behavior.update(camId, w, h, tracks, dtSec);

            QVector<TrackOverlay> overlays;

            // İsim verildi: ekrandaki insan(lar)ı bu kalıcı ID'ye bağla
            const int bindPid = (m_bindNameId ? m_bindNameId->exchange(0) : 0);
            if (bindPid > 0) {
                std::vector<const Track *> humans;
                for (const auto &t : tracks) {
                    if (t.kind == ObjectKind::Human) {
                        humans.push_back(&t);
                    }
                }
                const Track *target = nullptr;
                if (humans.size() == 1) {
                    target = humans[0];
                } else if (!humans.empty()) {
                    // Zaten bu ID'ye map'li olan; yoksa en büyük kutu (yakın kamera)
                    for (const Track *h : humans) {
                        auto it = sessionToPersistent.find(h->id);
                        if (it != sessionToPersistent.end() && it->second == bindPid) {
                            target = h;
                            break;
                        }
                    }
                    if (!target) {
                        float bestArea = -1.f;
                        for (const Track *h : humans) {
                            const float a = (h->x2 - h->x1) * (h->y2 - h->y1);
                            if (a > bestArea) {
                                bestArea = a;
                                target = h;
                            }
                        }
                    }
                }
                if (target) {
                    // Bu track başka bir kalıcı ID'deyse üzerine yaz (003→005 isimli)
                    sessionToPersistent[target->id] = bindPid;
                    sessionBoxes[target->id] = {target->x1, target->y1, target->x2, target->y2};
                    // Aynı bindPid'yi başka track'ten kaldır
                    for (auto it = sessionToPersistent.begin(); it != sessionToPersistent.end();) {
                        if (it->second == bindPid && it->first != target->id) {
                            sessionBoxes.erase(it->first);
                            it = sessionToPersistent.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    // Ghost temizliği: bu pid için eski hayaletler
                    ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(),
                                                [bindPid](const GhostSlot &g) { return g.pid == bindPid; }),
                                 ghosts.end());
                }
            }

            // Kaybolan track → ghost (kısa süre aynı yerde yeniden eşle → 007/008 çoğalmasın)
            {
                std::set<int> alive;
                for (const auto &t : tracks) {
                    alive.insert(t.id);
                }
                for (auto it = sessionToPersistent.begin(); it != sessionToPersistent.end();) {
                    if (alive.count(it->first)) {
                        ++it;
                        continue;
                    }
                    GhostSlot g;
                    g.pid = it->second;
                    auto bit = sessionBoxes.find(it->first);
                    if (bit != sessionBoxes.end()) {
                        g.x1 = bit->second[0];
                        g.y1 = bit->second[1];
                        g.x2 = bit->second[2];
                        g.y2 = bit->second[3];
                        g.ttl = 40; // ~20 sn @ 2fps
                        ghosts.push_back(g);
                    }
                    sessionBoxes.erase(it->first);
                    framesSeen.erase(it->first);
                    it = sessionToPersistent.erase(it);
                }
                for (GhostSlot &g : ghosts) {
                    --g.ttl;
                }
                ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(),
                                            [](const GhostSlot &g) { return g.ttl <= 0; }),
                             ghosts.end());
            }

            std::set<int> livePersistentIds;
            for (const auto &t : tracks) {
                if (t.kind != ObjectKind::Human) {
                    continue;
                }
                auto it = sessionToPersistent.find(t.id);
                if (it != sessionToPersistent.end()) {
                    livePersistentIds.insert(it->second);
                }
            }

            for (const auto &t : tracks) {
                if (t.kind == ObjectKind::Ignore) {
                    continue;
                }

                int displayId = t.id;
                framesSeen[t.id] += 1;
                QString personName;
                bool named = false;

                if (t.kind == ObjectKind::Human && m_people) {
                    sessionBoxes[t.id] = {t.x1, t.y1, t.x2, t.y2};

                    // Ghost'tan miras (tracker ID değişse bile aynı kalıcı ID)
                    if (!sessionToPersistent.count(t.id)) {
                        int bestGi = -1;
                        float bestIou = 0.25f;
                        for (int gi = 0; gi < static_cast<int>(ghosts.size()); ++gi) {
                            if (livePersistentIds.count(ghosts[gi].pid)) {
                                continue;
                            }
                            const float s = boxIou(t.x1, t.y1, t.x2, t.y2, ghosts[gi].x1, ghosts[gi].y1,
                                                   ghosts[gi].x2, ghosts[gi].y2);
                            if (s > bestIou) {
                                bestIou = s;
                                bestGi = gi;
                            }
                        }
                        if (bestGi >= 0) {
                            sessionToPersistent[t.id] = ghosts[bestGi].pid;
                            livePersistentIds.insert(ghosts[bestGi].pid);
                            ghosts.erase(ghosts.begin() + bestGi);
                        }
                    }

                    const int seen = framesSeen[t.id];
                    // Tek resolve denemesi — crop oturunca (çok deneme = yeni ID riski)
                    const bool tryResolve = !sessionToPersistent.count(t.id) && seen == 6;
                    if (tryResolve) {
                        const int x1 = (std::max)(0, static_cast<int>(t.x1));
                        const int y1 = (std::max)(0, static_cast<int>(t.y1));
                        const int x2 = (std::min)(bgr.cols - 1, static_cast<int>(t.x2));
                        const int y2 = (std::min)(bgr.rows - 1, static_cast<int>(t.y2));
                        if (x2 > x1 + 4 && y2 > y1 + 4) {
                            cv::Mat crop = bgr(cv::Rect(x1, y1, x2 - x1, y2 - y1)).clone();
                            const int pid = m_people->resolve(crop, livePersistentIds);
                            if (pid > 0) {
                                sessionToPersistent[t.id] = pid;
                                livePersistentIds.insert(pid);
                            }
                        }
                    }
                    if (sessionToPersistent.count(t.id)) {
                        displayId = sessionToPersistent[t.id];
                        personName = m_people->nameOf(displayId);
                        named = !personName.isEmpty();
                    }
                }

                TrackOverlay o;
                o.id = displayId;
                o.box = QRectF(t.x1, t.y1, t.x2 - t.x1, t.y2 - t.y1);
                o.label = QString::fromStdString(t.labelTr);
                o.personName = personName;
                o.named = named;
                o.kind = t.kind;
                o.level = levelForKind(t.kind);

                const int alertKey =
                    camId * 1000000 + ((t.kind == ObjectKind::Human) ? (100000 + displayId)
                                                                    : (static_cast<int>(t.kind) * 10000 + t.id));
                if (!alertedKeys.count(alertKey) && framesSeen[t.id] >= (t.kind == ObjectKind::Cow ? 2 : 5)
                    && (t.kind != ObjectKind::Human || sessionToPersistent.count(t.id))) {
                    alertedKeys.insert(alertKey);
                    AlertEvent a;
                    a.cameraId = camId;
                    a.trackId = displayId;
                    a.kind = t.kind;
                    a.level = o.level;
                    a.personName = personName;
                    a.named = named;
                    a.timeText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
                    if (t.kind == ObjectKind::Human) {
                        a.title = named ? QStringLiteral("%1 ALGILANDI").arg(personName.toUpper())
                                        : QStringLiteral("İNSAN ALGILANDI");
                        a.detail = named
                                       ? QStringLiteral("İsimli kişi — kalıcı ID %1")
                                             .arg(displayId, 3, 10, QChar('0'))
                                       : QStringLiteral("Kalıcı ID %1 — soldan isim verebilirsiniz")
                                             .arg(displayId, 3, 10, QChar('0'));
                    } else if (t.kind == ObjectKind::Cow) {
                        a.title = QStringLiteral("İNEK ALGILANDI");
                        a.detail = QStringLiteral("Hedef hayvan — yeşil takip");
                    } else {
                        a.title = QStringLiteral("BAŞKA CANLI");
                        a.detail = QStringLiteral("Hareket eden yabancı canlı");
                    }
                    if (m_rings[camId]) {
                        a.clip = m_rings[camId]->lastSeconds(5, 4);
                    }
                    emit alertRaised(a);
                }

                overlays.push_back(o);
            }

            emit frameReady(QImage(), overlays);

            // Inference sonrası dinlen — sürekli full-speed YOLO CPU'yu %100 yapıyordu
            const qint64 took = throttle.elapsed();
            if (took < intervalMs) {
                QThread::msleep(static_cast<unsigned long>(intervalMs - took));
            } else {
                QThread::msleep(5);
            }
        }
    }

    std::array<FrameRingBuffer *, PipelineController::kMaxCameras> m_rings{};
    std::atomic<int> *m_activeId = nullptr;

    std::atomic<bool> m_stop{true};
    std::atomic<bool> m_restartModel{false};
    std::atomic<bool> m_busy{false};

    QString m_modelPath;
    std::mutex m_modelMutex;

    PersonIdentityStore *m_people = nullptr;
    std::atomic<int> *m_bindNameId = nullptr;
    FarmEyeStore *m_farm = nullptr;
    BehaviorAnalyzer m_behavior;
};

// ---------------------------------------------------------------------------
// Controller
// ---------------------------------------------------------------------------
PipelineController::PipelineController(QObject *parent)
    : QObject(parent)
{
    for (int i = 0; i < kMaxCameras; ++i) {
        m_rings[i] = std::make_unique<FrameRingBuffer>(48);
        m_uris[i] = QString();
        m_captureWorkers[i] = nullptr;
    }

    qRegisterMetaType<farmeye::AlertEvent>("farmeye::AlertEvent");
    qRegisterMetaType<farmeye::TrackOverlay>("farmeye::TrackOverlay");
    qRegisterMetaType<QVector<farmeye::TrackOverlay>>("QVector<farmeye::TrackOverlay>");

    m_uiTimer = new QTimer(this);
    m_uiTimer->setInterval(100); // ~10 fps UI — yeterli akıcılık, düşük CPU
    connect(m_uiTimer, &QTimer::timeout, this, &PipelineController::pollUiFrames);
}

PipelineController::~PipelineController()
{
    pause();

    for (int i = 0; i < kMaxCameras; ++i) {
        if (m_captureThreads[i].isRunning()) {
            m_captureThreads[i].quit();
            m_captureThreads[i].wait(3000);
        }
    }
    if (m_inferThread.isRunning()) {
        m_inferThread.quit();
        m_inferThread.wait(3000);
    }
}

FrameRingBuffer *PipelineController::ring(int camId)
{
    if (camId < 0 || camId >= kMaxCameras) {
        return nullptr;
    }
    return m_rings[camId].get();
}

void PipelineController::setCameraUri(int camId, const QString &uri)
{
    if (camId < 0 || camId >= kMaxCameras) {
        return;
    }
    m_uris[camId] = uri;
    if (m_captureWorkers[camId]) {
        QMetaObject::invokeMethod(m_captureWorkers[camId], "setUri", Qt::DirectConnection, Q_ARG(QString, uri));
    }
}

void PipelineController::setActiveCamera(int camId)
{
    if (camId < 0 || camId >= kMaxCameras) {
        return;
    }
    m_activeId.store(camId);
}

QString PipelineController::currentUri(int camId) const
{
    if (camId < 0 || camId >= kMaxCameras) {
        return QString();
    }
    return m_uris[camId];
}

void PipelineController::pollUiFrames()
{
    if (!m_running) {
        return;
    }
    const int active = m_activeId.load();
    ++m_uiTick;

    for (int i = 0; i < kMaxCameras; ++i) {
        if (m_uris[i].isEmpty() || !m_rings[i]) {
            continue;
        }
        // Aktif: her tick (~10fps). Diğerleri: ~2.5fps
        const bool refresh = (i == active) || ((m_uiTick + i) % 4 == 0);
        if (!refresh) {
            continue;
        }
        QImage img;
        if (m_rings[i]->tryLatestQImage(img)) {
            emit thumbFrameReady(i, img);
        }
    }
}

void PipelineController::ensureWorkers()
{
    if (!m_inferWorker) {
        std::array<FrameRingBuffer *, kMaxCameras> ringPtrs{};
        for (int i = 0; i < kMaxCameras; ++i) {
            ringPtrs[i] = m_rings[i].get();
        }

        m_inferWorker = new InferenceWorker(ringPtrs, &m_activeId, &m_people, &m_bindNameId, &m_farm);
        m_inferWorker->moveToThread(&m_inferThread);

        connect(&m_inferThread, &QThread::finished, m_inferWorker, &QObject::deleteLater);
        connect(m_inferWorker, &InferenceWorker::frameReady, this, &PipelineController::frameReady);
        connect(m_inferWorker, &InferenceWorker::alertRaised, this, &PipelineController::alertRaised);
        connect(m_inferWorker, &InferenceWorker::statusMessage, this, &PipelineController::statusMessage);
        connect(m_inferWorker, &InferenceWorker::failed, this, [this](const QString &reason) {
            emit statusMessage(reason);
            m_running = false;
            emit runningChanged(false);
            if (m_uiTimer) {
                m_uiTimer->stop();
            }
        });
        m_inferThread.start();
    }

    for (int i = 0; i < kMaxCameras; ++i) {
        if (m_uris[i].isEmpty()) {
            continue;
        }
        if (m_captureWorkers[i]) {
            continue;
        }
        m_captureWorkers[i] = new CaptureWorker(i, m_rings[i].get(), m_uris[i]);
        m_captureWorkers[i]->moveToThread(&m_captureThreads[i]);
        connect(&m_captureThreads[i], &QThread::finished, m_captureWorkers[i], &QObject::deleteLater);
        connect(m_captureWorkers[i], &CaptureWorker::statusMessage, this, &PipelineController::statusMessage);
        connect(m_captureWorkers[i], &CaptureWorker::failed, this, [this](const QString &reason) {
            emit statusMessage(reason);
        });
        m_captureThreads[i].start();
    }
}

void PipelineController::start()
{
    ensureWorkers();

    if (m_inferWorker) {
        QMetaObject::invokeMethod(m_inferWorker, "setModelPath", Qt::DirectConnection, Q_ARG(QString, m_modelPath));
        QMetaObject::invokeMethod(m_inferWorker, "startLoop", Qt::QueuedConnection);
    }

    for (int i = 0; i < kMaxCameras; ++i) {
        if (m_captureWorkers[i]) {
            QMetaObject::invokeMethod(m_captureWorkers[i], "startLoop", Qt::QueuedConnection);
        }
    }

    m_running = true;
    emit runningChanged(true);
    if (m_uiTimer) {
        m_uiTimer->start();
    }
}

void PipelineController::pause()
{
    for (int i = 0; i < kMaxCameras; ++i) {
        if (m_captureWorkers[i]) {
            QMetaObject::invokeMethod(m_captureWorkers[i], "stopLoop", Qt::DirectConnection);
        }
    }
    if (m_inferWorker) {
        QMetaObject::invokeMethod(m_inferWorker, "stopLoop", Qt::DirectConnection);
    }
    if (m_uiTimer) {
        m_uiTimer->stop();
    }
    m_running = false;
    emit runningChanged(false);
}

void PipelineController::setModelPath(const QString &path)
{
    m_modelPath = path;
    if (m_inferWorker) {
        QMetaObject::invokeMethod(m_inferWorker, "setModelPath", Qt::DirectConnection, Q_ARG(QString, path));
    }
}

bool PipelineController::setPersonName(int persistentId, const QString &name)
{
    if (!m_people.setName(persistentId, name)) {
        return false;
    }
    // İsim verilen ID'yi canlı takip kutusuna zorla bağla (003 ekranda, 005'e isim → 005 olsun)
    m_bindNameId.store(persistentId);
    return true;
}

QString PipelineController::personName(int persistentId) const
{
    return m_people.nameOf(persistentId);
}

QString PipelineController::peopleStorageHint() const
{
    return m_people.storageHint();
}

QString PipelineController::farmStorageHint() const
{
    return m_farm.storageHint();
}

} // namespace farmeye

#include "pipeline_controller.moc"
