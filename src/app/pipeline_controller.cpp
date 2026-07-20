#include "pipeline_controller.h"

#include "app/person_identity_store.h"
#include "engine/GstCameraCapture.h"
#include "engine/InferenceEngine.h"
#include "engine/class_policy.h"
#include "engine/preprocess.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QMetaType>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <map>
#include <opencv2/imgproc.hpp>
#include <set>

namespace farmeye {

namespace {

QImage matToQImage(const cv::Mat &bgr)
{
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888)
        .copy();
}

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

cv::Scalar colorForKind(ObjectKind k)
{
    switch (k) {
    case ObjectKind::Human:
        return cv::Scalar(0, 200, 255);
    case ObjectKind::OtherAnimal:
        return cv::Scalar(40, 40, 220);
    case ObjectKind::Cow:
        return cv::Scalar(80, 200, 80);
    default:
        return cv::Scalar(180, 180, 180);
    }
}

} // namespace

class PipelineWorker : public QObject {
    Q_OBJECT
public:
    explicit PipelineWorker(FrameRingBuffer *ring)
        : m_ring(ring)
        , m_people(std::string())
    {
    }

public slots:
    void setUri(const QString &uri)
    {
        m_uri = uri;
        m_restart = true;
    }
    void setModelPath(const QString &path) { m_modelPath = path; }
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
    void run()
    {
        InferenceEngine engine;
        QString model = m_modelPath;
        if (model.isEmpty()) {
#if defined(FARMEYE_SOURCE_DIR)
            model = QStringLiteral(FARMEYE_SOURCE_DIR) + QStringLiteral("/models/yolov8n_fp16.engine");
#else
            model = QStringLiteral("models/yolov8n_fp16.engine");
#endif
        }
        if (!engine.loadEngine(model.toStdString())) {
            emit statusMessage(QStringLiteral("Model yüklenemedi"));
            emit failed(QStringLiteral("Model yüklenemedi"));
            return;
        }

        GstCameraCapture cap;
        Tracker tracker;
        std::map<int, int> sessionToPersistent;
        std::set<int> alertedKeys;
        std::map<int, int> framesSeen;

        while (!m_stop) {
            if (m_restart || !cap.isOpen()) {
                cap.close();
                const QString uri = m_uri.isEmpty() ? QStringLiteral("webcam") : m_uri;
                if (!cap.open(uri.toStdString())) {
                    emit statusMessage(QStringLiteral("Kamera açılamadı: ") + uri);
                    QThread::msleep(1000);
                    continue;
                }
                m_restart = false;
                emit statusMessage(QStringLiteral("Kaynak: ") + uri);
            }

            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);

            FrameBuffer fb;
            if (!cap.read(fb)) {
                QThread::msleep(10);
                continue;
            }

            cv::Mat bgr(fb.height, fb.width, CV_8UC3, fb.data.data());
            LetterboxMeta meta;
            cv::Mat input = letterbox(bgr, meta, 640);

            std::vector<Detection> dets;
            engine.infer(input, dets);
            for (auto &d : dets) {
                mapBoxToOriginal(d.x1, d.y1, d.x2, d.y2, meta);
            }
            auto classified = applyClassPolicy(dets);
            auto tracks = tracker.update(classified);

            QVector<TrackOverlay> overlays;
            cv::Mat draw = bgr.clone();
            for (const auto &t : tracks) {
                if (t.kind == ObjectKind::Ignore) {
                    continue;
                }

                int displayId = t.id;
                framesSeen[t.id] += 1;

                if (t.kind == ObjectKind::Human && framesSeen[t.id] >= 8) {
                    if (!sessionToPersistent.count(t.id)) {
                        const int x1 = (std::max)(0, static_cast<int>(t.x1));
                        const int y1 = (std::max)(0, static_cast<int>(t.y1));
                        const int x2 = (std::min)(bgr.cols - 1, static_cast<int>(t.x2));
                        const int y2 = (std::min)(bgr.rows - 1, static_cast<int>(t.y2));
                        if (x2 > x1 + 4 && y2 > y1 + 4) {
                            cv::Mat crop = bgr(cv::Rect(x1, y1, x2 - x1, y2 - y1)).clone();
                            sessionToPersistent[t.id] = m_people.resolve(crop);
                        }
                    }
                    if (sessionToPersistent.count(t.id)) {
                        displayId = sessionToPersistent[t.id];
                    }
                }

                TrackOverlay o;
                o.id = displayId;
                o.box = QRectF(t.x1, t.y1, t.x2 - t.x1, t.y2 - t.y1);
                o.label = QString::fromStdString(t.labelTr);
                o.kind = t.kind;
                o.level = levelForKind(t.kind);

                const int alertKey =
                    (t.kind == ObjectKind::Human) ? (100000 + displayId)
                                                  : (static_cast<int>(t.kind) * 10000 + t.id);
                if (!alertedKeys.count(alertKey) && framesSeen[t.id] >= 5) {
                    alertedKeys.insert(alertKey);
                    AlertEvent a;
                    a.trackId = displayId;
                    a.kind = t.kind;
                    a.level = o.level;
                    a.timeText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
                    if (t.kind == ObjectKind::Human) {
                        a.title = QStringLiteral("İNSAN ALGILANDI");
                        a.detail = QStringLiteral("Kalıcı ID — tekrar görünce aynı kalır");
                    } else if (t.kind == ObjectKind::Cow) {
                        a.title = QStringLiteral("İNEK");
                        a.detail = QStringLiteral("Hedef hayvan izleniyor");
                    } else {
                        a.title = QStringLiteral("BAŞKA CANLI");
                        a.detail = QStringLiteral("Hareket eden yabancı canlı");
                    }
                    if (m_ring) {
                        a.clip = m_ring->lastSeconds(5, 12);
                    }
                    emit alertRaised(a);
                }

                const cv::Scalar color = colorForKind(t.kind);
                cv::rectangle(draw,
                              cv::Rect(static_cast<int>(t.x1), static_cast<int>(t.y1),
                                       static_cast<int>(t.x2 - t.x1), static_cast<int>(t.y2 - t.y1)),
                              color, 2);
                const std::string tag = "ID: " + std::to_string(displayId);
                cv::putText(draw, tag, cv::Point(static_cast<int>(t.x1), std::max(15, static_cast<int>(t.y1) - 6)),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);
                overlays.push_back(o);
            }

            QImage img = matToQImage(draw);
            if (m_ring) {
                m_ring->push(img);
            }
            emit frameReady(img, overlays);
        }

        cap.close();
    }

    FrameRingBuffer *m_ring = nullptr;
    PersonIdentityStore m_people;
    QString m_uri = QStringLiteral("webcam");
    QString m_modelPath;
    std::atomic<bool> m_stop{true};
    std::atomic<bool> m_restart{false};
    std::atomic<bool> m_busy{false};
};

PipelineController::PipelineController(QObject *parent)
    : QObject(parent)
    , m_ring(150)
{
    qRegisterMetaType<farmeye::AlertEvent>("farmeye::AlertEvent");
    qRegisterMetaType<farmeye::TrackOverlay>("farmeye::TrackOverlay");
    qRegisterMetaType<QVector<farmeye::TrackOverlay>>("QVector<farmeye::TrackOverlay>");
}

PipelineController::~PipelineController()
{
    pause();
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait(3000);
    }
    m_worker = nullptr;
}

void PipelineController::ensureWorker()
{
    if (m_worker) {
        return;
    }
    m_worker = new PipelineWorker(&m_ring);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &PipelineWorker::frameReady, this, &PipelineController::frameReady);
    connect(m_worker, &PipelineWorker::alertRaised, this, &PipelineController::alertRaised);
    connect(m_worker, &PipelineWorker::statusMessage, this, &PipelineController::statusMessage);
    connect(m_worker, &PipelineWorker::failed, this, [this](const QString &reason) {
        emit statusMessage(reason);
        m_running = false;
        emit runningChanged(false);
    });
    m_thread.start();
}

void PipelineController::start()
{
    ensureWorker();
    if (!m_thread.isRunning()) {
        m_thread.start();
    }
    QMetaObject::invokeMethod(m_worker, "setUri", Qt::QueuedConnection, Q_ARG(QString, m_uri));
    QMetaObject::invokeMethod(m_worker, "setModelPath", Qt::QueuedConnection, Q_ARG(QString, m_modelPath));
    QMetaObject::invokeMethod(m_worker, "startLoop", Qt::QueuedConnection);
    m_running = true;
    emit runningChanged(true);
}

void PipelineController::pause()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stopLoop", Qt::DirectConnection);
    }
    m_running = false;
    emit runningChanged(false);
}

void PipelineController::setCameraUri(const QString &uri)
{
    m_uri = uri;
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setUri", Qt::QueuedConnection, Q_ARG(QString, uri));
    }
}

void PipelineController::setModelPath(const QString &path)
{
    m_modelPath = path;
}

} // namespace farmeye

#include "pipeline_controller.moc"
