#include "mainwindow.h"

#include "alert_list_panel.h"
#include "camera_grid_panel.h"
#include "clip_player_dialog.h"
#include "system_status_bar.h"
#include "video_gl_widget.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace farmeye {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("FARMEYE™ AI"));
    resize(1600, 960);
    buildUi();
    loadTheme();
    loadCamerasConfig();

    connect(&m_pipeline, &PipelineController::frameReady, this,
            [this](const QImage &, const QVector<farmeye::TrackOverlay> &tracks) {
                m_video->setTracks(tracks);
            });
    connect(&m_pipeline, &PipelineController::thumbFrameReady, this,
            [this](int camId, const QImage &frame) {
                m_cameras->setThumbFrame(camId, frame);
                if (camId == m_cameras->activeId()) {
                    m_video->setImage(frame);
                }
            });
    connect(&m_pipeline, &PipelineController::alertRaised, this, &MainWindow::onAlert);
    connect(&m_pipeline, &PipelineController::runningChanged, this, &MainWindow::onRunningChanged);
    connect(&m_pipeline, &PipelineController::statusMessage, m_statusMsg, &QLabel::setText);

    connect(m_alerts, &AlertListPanel::playClipRequested, this, &MainWindow::onPlayClip);
    connect(m_alerts, &AlertListPanel::namePersonRequested, this, &MainWindow::onNamePerson);
    connect(m_cameras, &CameraGridPanel::cameraSelected, this, &MainWindow::onCameraSelected);

    connect(&m_sysMonitor, &SystemMonitor::statsUpdated, m_statusBar, &SystemStatusBar::updateStats);
    auto *sysTimer = new QTimer(this);
    connect(sysTimer, &QTimer::timeout, &m_sysMonitor, &SystemMonitor::tick);
    sysTimer->start(1000);
    m_sysMonitor.tick();

    onRunningChanged(false);
}

MainWindow::~MainWindow()
{
    m_pipeline.pause();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralRoot"));
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(12);

    // Top bar
    auto *top = new QFrame(central);
    top->setObjectName(QStringLiteral("topBar"));
    auto *topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(18, 12, 18, 12);
    topLay->setSpacing(12);

    auto *brandMark = new QLabel(QStringLiteral("F"), top);
    brandMark->setObjectName(QStringLiteral("brandMark"));
    brandMark->setAlignment(Qt::AlignCenter);
    auto *brand = new QLabel(QStringLiteral("FARMEYE™ AI"), top);
    brand->setObjectName(QStringLiteral("brandLabel"));

    auto *brandCol = new QHBoxLayout;
    brandCol->setSpacing(10);
    brandCol->addWidget(brandMark);
    brandCol->addWidget(brand);

    m_statusBar = new SystemStatusBar(top);
    topLay->addLayout(brandCol);
    topLay->addSpacing(8);
    topLay->addWidget(m_statusBar, 1);
    root->addWidget(top);

    // Middle: left | center | right
    auto *mid = new QHBoxLayout;
    mid->setSpacing(14);

    // Left column
    auto *left = new QFrame(central);
    left->setObjectName(QStringLiteral("panel"));
    left->setFixedWidth(340);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(16, 16, 16, 16);
    leftLay->setSpacing(12);

    auto *ctrlTitle = new QLabel(QStringLiteral("KONTROL MERKEZİ"), left);
    ctrlTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto *ana = new QLabel(QStringLiteral("ANA KONTROL"), left);
    ana->setObjectName(QStringLiteral("subSection"));

    auto *ctrlBox = new QFrame(left);
    ctrlBox->setObjectName(QStringLiteral("ctrlBox"));
    auto *ctrlInner = new QVBoxLayout(ctrlBox);
    ctrlInner->setContentsMargins(12, 12, 12, 12);
    ctrlInner->setSpacing(10);
    m_btnStart = new QPushButton(QStringLiteral("▶    BAŞLAT"), ctrlBox);
    m_btnStart->setObjectName(QStringLiteral("btnStart"));
    m_btnStart->setCursor(Qt::PointingHandCursor);
    m_btnPause = new QPushButton(QStringLiteral("❚❚   DURAKLAT"), ctrlBox);
    m_btnPause->setObjectName(QStringLiteral("btnPause"));
    m_btnPause->setCursor(Qt::PointingHandCursor);
    m_aiStatus = new QLabel(QStringLiteral("●  YAPAY ZEKA BEKLEMEDE"), ctrlBox);
    m_aiStatus->setObjectName(QStringLiteral("aiActive"));
    m_statusMsg = new QLabel(QStringLiteral(""), ctrlBox);
    m_statusMsg->setWordWrap(true);
    m_statusMsg->setStyleSheet(QStringLiteral("color:#7f93a8; font-size:11px;"));

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);
    btnRow->addWidget(m_btnStart, 1);
    btnRow->addWidget(m_btnPause, 1);
    ctrlInner->addLayout(btnRow);
    ctrlInner->addWidget(m_aiStatus);
    ctrlInner->addWidget(m_statusMsg);

    m_alerts = new AlertListPanel(left);
    leftLay->addWidget(ctrlTitle);
    leftLay->addWidget(ana);
    leftLay->addWidget(ctrlBox);
    leftLay->addWidget(m_alerts, 1);

    // Center video
    auto *center = new QFrame(central);
    center->setObjectName(QStringLiteral("videoFrame"));
    auto *centerLay = new QVBoxLayout(center);
    centerLay->setContentsMargins(4, 4, 4, 4);
    m_video = new VideoGLWidget(center);
    centerLay->addWidget(m_video, 1);

    // Right cameras
    auto *right = new QFrame(central);
    right->setObjectName(QStringLiteral("panel"));
    right->setFixedWidth(300);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(16, 16, 16, 16);
    m_cameras = new CameraGridPanel(right);
    rightLay->addWidget(m_cameras);

    mid->addWidget(left);
    mid->addWidget(center, 1);
    mid->addWidget(right);
    root->addLayout(mid, 1);

    // Bottom bar
    auto *bottom = new QFrame(central);
    bottom->setObjectName(QStringLiteral("bottomBar"));
    auto *botLay = new QHBoxLayout(bottom);
    botLay->setContentsMargins(16, 10, 16, 10);
    auto *resLab = new QLabel(QStringLiteral("GÖRÜNTÜ ÇÖZÜNÜRLÜĞÜ"), bottom);
    resLab->setObjectName(QStringLiteral("subSection"));
    m_resolution = new QComboBox(bottom);
    m_resolution->addItems({QStringLiteral("720p"), QStringLiteral("1080p"), QStringLiteral("1440p")});
    m_resolution->setCurrentIndex(1);
    auto *setLab = new QLabel(QStringLiteral("GÖRÜNTÜ AYARLARI"), bottom);
    setLab->setObjectName(QStringLiteral("subSection"));
    auto *btnPc = new QPushButton(QStringLiteral("⚙  PC'YE GÖRE AYARLAMA"), bottom);
    btnPc->setObjectName(QStringLiteral("btnPc"));
    botLay->addWidget(resLab);
    botLay->addWidget(m_resolution);
    botLay->addStretch();
    botLay->addWidget(setLab);
    botLay->addWidget(btnPc);
    root->addWidget(bottom);

    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_btnPause, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(m_resolution, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onResolutionChanged);
    connect(btnPc, &QPushButton::clicked, this, &MainWindow::onPcPreset);
}

void MainWindow::loadTheme()
{
    QFile f(QStringLiteral(":/theme.qss"));
#if defined(FARMEYE_SOURCE_DIR)
    if (!f.exists()) {
        f.setFileName(QStringLiteral(FARMEYE_SOURCE_DIR) + QStringLiteral("/src/ui/theme.qss"));
    }
#endif
    if (f.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    }
}

void MainWindow::loadCamerasConfig()
{
    QString path = QStringLiteral("config/cameras.json");
#if defined(FARMEYE_SOURCE_DIR)
    path = QStringLiteral(FARMEYE_SOURCE_DIR) + QStringLiteral("/config/cameras.json");
#endif
    QFile f(path);
    QVector<CameraSlot> camList;
    if (f.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        const auto arr = doc.object().value(QStringLiteral("cameras")).toArray();
        for (const auto &v : arr) {
            const auto o = v.toObject();
            CameraSlot s;
            s.id = o.value(QStringLiteral("id")).toInt();
            s.name = o.value(QStringLiteral("name")).toString();
            s.uri = o.value(QStringLiteral("uri")).toString();
            s.enabled = o.value(QStringLiteral("enabled")).toBool();
            camList.push_back(s);
        }
    }
    if (camList.isEmpty()) {
        CameraSlot s;
        s.id = 0;
        s.name = QStringLiteral("Kamera 1");
        s.uri = QStringLiteral("webcam:0");
        s.enabled = true;
        camList.push_back(s);
    }
    m_cameras->setCameras(camList);

    int firstActive = -1;
    for (const auto &s : camList) {
        if (s.enabled && !s.uri.isEmpty()) {
            m_pipeline.setCameraUri(s.id, s.uri);
            if (firstActive < 0) {
                firstActive = s.id;
            }
        } else {
            m_pipeline.setCameraUri(s.id, QString());
        }
    }
    if (firstActive >= 0) {
        m_pipeline.setActiveCamera(firstActive);
    } else {
        m_pipeline.setActiveCamera(0);
    }
}

void MainWindow::onStart()
{
    m_pipeline.start();
}

void MainWindow::onPause()
{
    m_pipeline.pause();
}

void MainWindow::onRunningChanged(bool running)
{
    m_btnStart->setEnabled(!running);
    m_btnPause->setEnabled(running);
    if (running) {
        m_aiStatus->setText(QStringLiteral("● YAPAY ZEKA AKTİF"));
        m_aiStatus->setStyleSheet(QStringLiteral("color:#3dd68c; font-weight:600;"));
    } else {
        m_aiStatus->setText(QStringLiteral("● YAPAY ZEKA BEKLEMEDE"));
        m_aiStatus->setStyleSheet(QStringLiteral("color:#9fb0c3; font-weight:600;"));
    }
}

void MainWindow::onAlert(AlertEvent alert)
{
    m_alerts->addAlert(alert);
}

void MainWindow::onPlayClip(AlertEvent alert)
{
    if (alert.clip.isEmpty()) {
        if (auto *r = m_pipeline.ring(alert.cameraId)) {
            alert.clip = r->lastSeconds(5, 12);
        }
    }
    auto *dlg = new ClipPlayerDialog(alert, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

bool MainWindow::promptAndSetPersonName(int persistentId, const QString &currentName)
{
    if (persistentId <= 0) {
        return false;
    }
    const QString existing = m_pipeline.personName(persistentId);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, existing.isEmpty() ? QStringLiteral("Kişiye isim ver") : QStringLiteral("İsim değiştir"),
        QStringLiteral("Kalıcı ID %1 için isim:\n(Bir ID = bir kişi; eski isim üzerine yazılır)")
            .arg(persistentId, 3, 10, QChar('0')),
        QLineEdit::Normal, currentName.isEmpty() ? existing : currentName, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return false;
    }
    if (!m_pipeline.setPersonName(persistentId, name.trimmed())) {
        QMessageBox::warning(this, QStringLiteral("İsim"),
                             QStringLiteral("Bu ID henüz kaydedilmedi. Biraz bekleyip tekrar deneyin."));
        return false;
    }
    m_statusMsg->setText(QStringLiteral("Kaydedildi: %1 → ID %2 (%3) — canlı kutuya bağlandı")
                             .arg(name.trimmed())
                             .arg(persistentId, 3, 10, QChar('0'))
                             .arg(m_pipeline.peopleStorageHint()));

    AlertEvent namedAlert;
    namedAlert.cameraId = m_cameras ? m_cameras->activeId() : 0;
    namedAlert.trackId = persistentId;
    namedAlert.kind = ObjectKind::Human;
    namedAlert.level = AlertLevel::Suspicious;
    namedAlert.named = true;
    namedAlert.personName = name.trimmed();
    namedAlert.title = existing.isEmpty()
                           ? QStringLiteral("%1 KAYDEDİLDİ").arg(name.trimmed().toUpper())
                           : QStringLiteral("%1 (eski: %2)").arg(name.trimmed().toUpper(), existing.toUpper());
    namedAlert.detail = QStringLiteral("Tek kalıcı ID %1 — ekrandaki kişiye bağlandı")
                            .arg(persistentId, 3, 10, QChar('0'));
    namedAlert.timeText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_alerts->addAlert(namedAlert);
    return true;
}

void MainWindow::onNamePerson(AlertEvent alert)
{
    if (alert.kind != ObjectKind::Human) {
        return;
    }
    promptAndSetPersonName(alert.trackId, alert.personName);
}

void MainWindow::onCameraSelected(int id, const QString &uri)
{
    if (uri.isEmpty()) {
        m_statusMsg->setText(QStringLiteral("Bu kamera slotu boş (BOŞTA)"));
        return;
    }
    m_pipeline.setActiveCamera(id);
    m_statusMsg->setText(QStringLiteral("Kamera: ") + uri);
}

void MainWindow::onResolutionChanged(int index)
{
    static const char *labels[] = {"720p", "1080p", "1440p"};
    const char *lab = (index >= 0 && index < 3) ? labels[index] : "1080p";
    m_statusMsg->setText(QStringLiteral("Çözünürlük preset: %1 (pipeline caps sonraki açılışta)").arg(lab));
}

void MainWindow::onPcPreset()
{
    // GTX 1650 / 8GB RAM için dengeli varsayılan
    m_resolution->setCurrentIndex(1); // 1080p
    m_statusMsg->setText(QStringLiteral("PC preset: 1080p, YOLO n, FP16 tercih"));
    QMessageBox::information(this, QStringLiteral("PC ayarı"),
                             QStringLiteral("Bu PC için 1080p + YOLOv8n önerildi."));
}

} // namespace farmeye
