#pragma once

#include "app/frame_ring_buffer.h"
#include "app/person_identity_store.h"
#include "engine/class_policy.h"
#include "tracking/tracker.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <atomic>
#include <memory>
#include <array>

namespace farmeye {

class CaptureWorker;
class InferenceWorker;

// UI rengi: Cow=yeşil, Human=sarı, OtherAnimal=kırmızı (kırmızı listede en üstte)
enum class AlertLevel { Normal = 0, Suspicious = 1, Abnormal = 2 };

struct AlertEvent {
    int cameraId = 0;
    int trackId = 0; // kalıcı ID (insan için); session tracker ID değil
    AlertLevel level = AlertLevel::Normal;
    ObjectKind kind = ObjectKind::Ignore;
    QString title;
    QString detail;
    QString timeText;
    QString personName; // kullanıcı etiketi (örn. Kadir)
    bool named = false; // isimli → mor kart
    QVector<QImage> clip;
};

struct TrackOverlay {
    int id = 0; // kalıcı ID (insan); diğerleri tracker ID
    QRectF box;
    QString label;
    QString personName;
    bool named = false;
    AlertLevel level = AlertLevel::Normal;
    ObjectKind kind = ObjectKind::Ignore;
};

class PipelineController : public QObject {
    Q_OBJECT
public:
    explicit PipelineController(QObject *parent = nullptr);
    ~PipelineController() override;

    static constexpr int kMaxCameras = 4;

    FrameRingBuffer *ring(int camId);
    FrameRingBuffer *ring() { return ring(m_activeId.load()); }

    void setCameraUri(int camId, const QString &uri);
    void setActiveCamera(int camId);
    QString currentUri(int camId) const;

public slots:
    void start();
    void pause();
    void setModelPath(const QString &path);

    // Kalıcı kişiye isim ver (disk: JSON + SQLite). Başarılıysa true.
    bool setPersonName(int persistentId, const QString &name);
    QString personName(int persistentId) const;
    QString peopleStorageHint() const;

signals:
    void frameReady(const QImage &frame, const QVector<farmeye::TrackOverlay> &tracks);
    void thumbFrameReady(int camId, const QImage &frame);
    void alertRaised(farmeye::AlertEvent alert);
    void runningChanged(bool running);
    void statusMessage(const QString &msg);

private slots:
    void pollUiFrames();

private:
    void ensureWorkers();

    QString m_modelPath;
    std::array<std::unique_ptr<FrameRingBuffer>, kMaxCameras> m_rings;
    std::array<QString, kMaxCameras> m_uris;

    std::array<QThread, kMaxCameras> m_captureThreads;
    std::array<CaptureWorker *, kMaxCameras> m_captureWorkers{};

    QThread m_inferThread;
    InferenceWorker *m_inferWorker = nullptr;

    QTimer *m_uiTimer = nullptr;
    int m_uiTick = 0;

    std::atomic<int> m_activeId{0};
    std::atomic<int> m_bindNameId{0}; // setPersonName → ekrandaki kişiyi bu kalıcı ID'ye bağla
    bool m_running = false;

    PersonIdentityStore m_people;
};

} // namespace farmeye

Q_DECLARE_METATYPE(farmeye::AlertEvent)
Q_DECLARE_METATYPE(farmeye::TrackOverlay)
Q_DECLARE_METATYPE(QVector<farmeye::TrackOverlay>)
