#include "system_monitor.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <QMetaType>
#include <QProcess>
#include <algorithm>

namespace farmeye {

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<farmeye::SystemStats>("farmeye::SystemStats");
}

void SystemMonitor::tick()
{
    SystemStats s;

#ifdef _WIN32
    FILETIME idleF, kernelF, userF;
    if (GetSystemTimes(&idleF, &kernelF, &userF)) {
        auto toU64 = [](const FILETIME &ft) -> quint64 {
            return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        const quint64 idle = toU64(idleF);
        const quint64 kernel = toU64(kernelF);
        const quint64 user = toU64(userF);
        const quint64 total = kernel + user;
        if (m_hasPrev) {
            const quint64 dIdle = idle - m_prevIdle;
            const quint64 dTotal = total - m_prevTotal;
            if (dTotal > 0) {
                s.cpuPercent = (1.0 - static_cast<double>(dIdle) / static_cast<double>(dTotal)) * 100.0;
            }
        }
        m_prevIdle = idle;
        m_prevTotal = total;
        m_hasPrev = true;
    }

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        s.ramPercent = static_cast<double>(mem.dwMemoryLoad);
    }
#endif

    QProcess p;
    p.start("nvidia-smi",
            {"--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"});
    if (p.waitForFinished(800)) {
        const QString out = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
        bool ok = false;
        const double g = out.section('\n', 0, 0).trimmed().toDouble(&ok);
        if (ok) {
            s.gpuPercent = g;
        }
    }

    s.netMbps = 0; // placeholder
    emit statsUpdated(s);
}

} // namespace farmeye
