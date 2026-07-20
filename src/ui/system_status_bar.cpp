#include "system_status_bar.h"
#include "sparkline_widget.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QTimer>
#include <QVBoxLayout>

namespace farmeye {

namespace {

QFrame *makeMetricChip(const QString &name, SparklineWidget **sparkOut, QLabel **valueOut,
                       const QColor &color, QWidget *parent)
{
    auto *chip = new QFrame(parent);
    chip->setObjectName(QStringLiteral("metricChip"));
    chip->setMinimumWidth(118);

    auto *nameLab = new QLabel(name, chip);
    nameLab->setObjectName(QStringLiteral("metricName"));
    auto *val = new QLabel(QStringLiteral("--"), chip);
    val->setObjectName(QStringLiteral("metricValue"));
    *valueOut = val;

    auto *spark = new SparklineWidget(chip);
    spark->setColor(color);
    spark->setFixedHeight(28);
    spark->setMinimumWidth(90);
    *sparkOut = spark;

    auto *top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(nameLab);
    top->addStretch();
    top->addWidget(val);

    auto *lay = new QVBoxLayout(chip);
    lay->setContentsMargins(10, 8, 10, 8);
    lay->setSpacing(4);
    lay->addLayout(top);
    lay->addWidget(spark);
    return chip;
}

} // namespace

SystemStatusBar::SystemStatusBar(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("SİSTEM ANALİZİ"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));

    QLabel *cpuVal = nullptr;
    QLabel *gpuVal = nullptr;
    QLabel *agVal = nullptr;
    auto *cpuChip = makeMetricChip(QStringLiteral("CPU"), &m_sparkCpu, &cpuVal, QColor(0x3d, 0xd6, 0x8c), this);
    auto *gpuChip = makeMetricChip(QStringLiteral("GPU"), &m_sparkGpu, &gpuVal, QColor(0xf0, 0xc1, 0x4a), this);
    auto *agChip = makeMetricChip(QStringLiteral("AĞ"), &m_sparkNet, &agVal, QColor(0x5b, 0x9b, 0xff), this);
    m_cpuVal = cpuVal;
    m_gpuVal = gpuVal;
    m_agVal = agVal;

    auto *chips = new QHBoxLayout;
    chips->setSpacing(10);
    chips->addWidget(cpuChip);
    chips->addWidget(gpuChip);
    chips->addWidget(agChip);

    m_clock = new QLabel(this);
    m_clock->setObjectName(QStringLiteral("sysClock"));
    m_clock->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(14);
    lay->addWidget(title);
    lay->addLayout(chips, 1);
    lay->addWidget(m_clock);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        QLocale tr(QLocale::Turkish);
        m_clock->setText(tr.toString(QDateTime::currentDateTime(), QStringLiteral("dd MMMM yyyy  |  HH:mm:ss")).toUpper());
    });
    timer->start(1000);
    QLocale tr(QLocale::Turkish);
    m_clock->setText(tr.toString(QDateTime::currentDateTime(), QStringLiteral("dd MMMM yyyy  |  HH:mm:ss")).toUpper());
}

void SystemStatusBar::updateStats(const SystemStats &stats)
{
    m_sparkCpu->push(stats.cpuPercent);
    m_sparkGpu->push(stats.gpuPercent);
    const double ag = stats.netMbps > 0.1 ? stats.netMbps : (stats.cpuPercent * 0.55 + stats.ramPercent * 0.25);
    m_sparkNet->push(ag);
    if (m_cpuVal) {
        m_cpuVal->setText(QStringLiteral("%1%").arg(stats.cpuPercent, 0, 'f', 0));
    }
    if (m_gpuVal) {
        m_gpuVal->setText(QStringLiteral("%1%").arg(stats.gpuPercent, 0, 'f', 0));
    }
    if (m_agVal) {
        m_agVal->setText(QStringLiteral("%1 Mbps").arg(ag, 0, 'f', 0));
    }
}

} // namespace farmeye
