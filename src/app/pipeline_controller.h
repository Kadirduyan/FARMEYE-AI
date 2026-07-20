#pragma once

#include "app/frame_ring_buffer.h"
#include "engine/class_policy.h"
#include "tracking/tracker.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVector>
#include <atomic>
#include <memory>

namespace farmeye {

// UI rengi: Cow=yeşil, Human=sarı, OtherAnimal=kırmızı (kırmızı listede en üstte)
enum class AlertLevel { Normal = 0, Suspicious = 1, Abnormal = 2 };

struct AlertEvent {
    int trackId = 0;
    AlertLevel level = AlertLevel::Normal;
    ObjectKind kind = ObjectKind::Ignore;
    QString title;
    QString detail;
    QString timeText;
    QVector<QImage> clip;
};

struct TrackOverlay {
    int id = 0;
    QRectF box;
    QString label;
    AlertLevel level = AlertLevel::Normal;
    ObjectKind kind = ObjectKind::Ignore;
};

class PipelineWorker;

class PipelineController : public QObject {
    Q_OBJECT
public:
    explicit PipelineController(QObject *parent = nullptr);
    ~PipelineController() override;

    FrameRingBuffer *ring() { return &m_ring; }
    QString currentUri() const { return m_uri; }

public slots:
    void start();
    void pause();
    void setCameraUri(const QString &uri);
    void setModelPath(const QString &path);

signals:
    void frameReady(const QImage &frame, const QVector<farmeye::TrackOverlay> &tracks);
    void alertRaised(farmeye::AlertEvent alert);
    void runningChanged(bool running);
    void statusMessage(const QString &msg);

private:
    void ensureWorker();

    QString m_uri = QStringLiteral("webcam");
    QString m_modelPath;
    FrameRingBuffer m_ring;
    QThread m_thread;
    PipelineWorker *m_worker = nullptr;
    bool m_running = false;
};

} // namespace farmeye

Q_DECLARE_METATYPE(farmeye::AlertEvent)
Q_DECLARE_METATYPE(farmeye::TrackOverlay)
Q_DECLARE_METATYPE(QVector<farmeye::TrackOverlay>)
