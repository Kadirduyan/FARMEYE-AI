#pragma once

#include <QObject>
#include <QString>

namespace farmeye {

struct SystemStats {
    double cpuPercent = 0;
    double ramPercent = 0;
    double gpuPercent = 0;
    double netMbps = 0;
};

class SystemMonitor : public QObject {
    Q_OBJECT
public:
    explicit SystemMonitor(QObject *parent = nullptr);

public slots:
    void tick();

signals:
    void statsUpdated(farmeye::SystemStats stats);

private:
    quint64 m_prevIdle = 0;
    quint64 m_prevTotal = 0;
    bool m_hasPrev = false;
};

} // namespace farmeye

Q_DECLARE_METATYPE(farmeye::SystemStats)
