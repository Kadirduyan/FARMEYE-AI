#pragma once

#include "app/system_monitor.h"

#include <QWidget>

class QLabel;

namespace farmeye {

class SparklineWidget;

class SystemStatusBar : public QWidget {
    Q_OBJECT
public:
    explicit SystemStatusBar(QWidget *parent = nullptr);

public slots:
    void updateStats(const farmeye::SystemStats &stats);

private:
    SparklineWidget *m_sparkCpu = nullptr;
    SparklineWidget *m_sparkGpu = nullptr;
    SparklineWidget *m_sparkNet = nullptr;
    QLabel *m_cpuVal = nullptr;
    QLabel *m_gpuVal = nullptr;
    QLabel *m_agVal = nullptr;
    QLabel *m_clock = nullptr;
};

} // namespace farmeye
