#pragma once

#include "app/pipeline_controller.h"
#include "app/system_monitor.h"

#include <QMainWindow>

class QComboBox;
class QLabel;
class QPushButton;

namespace farmeye {

class AlertListPanel;
class CameraGridPanel;
class SystemStatusBar;
class VideoGLWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStart();
    void onPause();
    void onRunningChanged(bool running);
    void onAlert(farmeye::AlertEvent alert);
    void onPlayClip(farmeye::AlertEvent alert);
    void onNamePerson(farmeye::AlertEvent alert);
    void onCameraSelected(int id, const QString &uri);
    void onResolutionChanged(int index);
    void onPcPreset();

private:
    void buildUi();
    void loadCamerasConfig();
    void loadTheme();
    bool promptAndSetPersonName(int persistentId, const QString &currentName);

    PipelineController m_pipeline;
    SystemMonitor m_sysMonitor;

    VideoGLWidget *m_video = nullptr;
    AlertListPanel *m_alerts = nullptr;
    CameraGridPanel *m_cameras = nullptr;
    SystemStatusBar *m_statusBar = nullptr;
    QPushButton *m_btnStart = nullptr;
    QPushButton *m_btnPause = nullptr;
    QLabel *m_aiStatus = nullptr;
    QLabel *m_statusMsg = nullptr;
    QComboBox *m_resolution = nullptr;
};

} // namespace farmeye
