#include "clip_player_dialog.h"

#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

namespace farmeye {

ClipPlayerDialog::ClipPlayerDialog(const AlertEvent &alert, QWidget *parent)
    : QDialog(parent)
    , m_frames(alert.clip)
{
    setWindowTitle(QStringLiteral("Cow %1 — 5s Önizleme").arg(alert.trackId, 3, 10, QChar('0')));
    resize(640, 420);

    m_info = new QLabel(QStringLiteral("ID: %1 | %2 | %3\n%4")
                            .arg(alert.trackId, 3, 10, QChar('0'))
                            .arg(alert.timeText)
                            .arg(alert.title)
                            .arg(alert.detail),
                        this);
    m_view = new QLabel(this);
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setMinimumSize(600, 340);
    m_view->setStyleSheet(QStringLiteral("background:#0b0f14; border-radius:8px;"));

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(m_info);
    lay->addWidget(m_view, 1);

    if (m_frames.isEmpty()) {
        m_view->setText(QStringLiteral("Klip buffer boş"));
        return;
    }

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (m_frames.isEmpty()) {
            return;
        }
        const QImage &img = m_frames[m_index % m_frames.size()];
        m_view->setPixmap(QPixmap::fromImage(img).scaled(m_view->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ++m_index;
        if (m_index >= m_frames.size()) {
            m_index = 0;
        }
    });
    m_timer->start(80);
}

} // namespace farmeye
