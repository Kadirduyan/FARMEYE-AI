#include "video_gl_widget.h"

#include <QMouseEvent>
#include <QMutexLocker>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QPainterPath>

namespace farmeye {

VideoGLWidget::VideoGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(640, 360);
}

void VideoGLWidget::setFrame(const QImage &frame, const QVector<TrackOverlay> &tracks)
{
    setImage(frame);
    setTracks(tracks);
}

void VideoGLWidget::setImage(const QImage &frame)
{
    {
        QMutexLocker lock(&m_mutex);
        m_frame = frame;
    }
    update();
}

void VideoGLWidget::setTracks(const QVector<TrackOverlay> &tracks)
{
    {
        QMutexLocker lock(&m_mutex);
        m_tracks = tracks;
        if (!tracks.isEmpty()) {
            bool keep = false;
            for (const auto &t : tracks) {
                if (t.id == m_focusId) {
                    keep = true;
                    break;
                }
            }
            if (!keep || m_focusId < 0) {
                m_focusId = tracks.first().id;
                for (const auto &t : tracks) {
                    if (t.kind == ObjectKind::OtherAnimal) {
                        m_focusId = t.id;
                        break;
                    }
                }
            }
        }
    }
    update();
}

void VideoGLWidget::initializeGL()
{
    if (auto *f = context()->functions()) {
        f->glClearColor(0.03f, 0.05f, 0.07f, 1.f);
    }
}

void VideoGLWidget::resizeGL(int w, int h)
{
    if (auto *f = context()->functions()) {
        f->glViewport(0, 0, w, h);
    }
}

static void drawMiniSpark(QPainter &p, const QRect &r, const QColor &c, int seed)
{
    QPainterPath path;
    for (int i = 0; i < 16; ++i) {
        const double t = i / 15.0;
        const double x = r.left() + t * r.width();
        const double y = r.bottom() - ((20 + ((seed + i * 11) % 50)) / 70.0) * r.height();
        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }
    p.setPen(QPen(c, 1.5));
    p.drawPath(path);
}

void VideoGLWidget::paintGL()
{
    QImage frame;
    QVector<TrackOverlay> tracks;
    int focusId = -1;
    {
        QMutexLocker lock(&m_mutex);
        frame = m_frame;
        tracks = m_tracks;
        focusId = m_focusId;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.fillRect(rect(), QColor(8, 12, 16));
    if (frame.isNull()) {
        p.setPen(QColor(120, 140, 160));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Görüntü yok — BAŞLAT"));
        return;
    }

    const QImage scaled = frame.scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    const int ox = (width() - scaled.width()) / 2;
    const int oy = (height() - scaled.height()) / 2;
    p.drawImage(ox, oy, scaled);

    // Kutuları orijinal kare koordinatlarından ekrana ölçekle
    if (!tracks.isEmpty() && frame.width() > 0 && frame.height() > 0) {
        const double sx = static_cast<double>(scaled.width()) / frame.width();
        const double sy = static_cast<double>(scaled.height()) / frame.height();
        for (const auto &t : tracks) {
            QColor penC(61, 214, 140);
            if (t.named) {
                penC = QColor(168, 85, 247); // mor — isimli kalıcı kişi
            } else if (t.kind == ObjectKind::Human || t.level == AlertLevel::Suspicious) {
                penC = QColor(240, 193, 74);
            } else if (t.kind == ObjectKind::OtherAnimal || t.level == AlertLevel::Abnormal) {
                penC = QColor(232, 93, 93);
            }
            const QRectF r(ox + t.box.x() * sx, oy + t.box.y() * sy, t.box.width() * sx,
                           t.box.height() * sy);
            p.setPen(QPen(penC, t.id == focusId ? 3 : 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
            const QString tag = t.named && !t.personName.isEmpty()
                                    ? QStringLiteral("%1 · %2").arg(t.personName).arg(t.id, 3, 10, QChar('0'))
                                    : QStringLiteral("ID: %1").arg(t.id, 3, 10, QChar('0'));
            p.drawText(r.topLeft() + QPointF(2, -4), tag);
        }
    }

    const TrackOverlay *focus = nullptr;
    for (const auto &t : tracks) {
        if (t.id == focusId) {
            focus = &t;
            break;
        }
    }
    if (!focus && !tracks.isEmpty()) {
        focus = &tracks.first();
    }
    if (!focus) {
        return;
    }

    // Glass detail card
    const QRect card(ox + 18, oy + 18, 280, 168);
    QString durum = QStringLiteral("İNEK");
    QColor accent(61, 214, 140);
    if (focus->named) {
        durum = focus->personName.isEmpty() ? QStringLiteral("İSİMLİ") : focus->personName.toUpper();
        accent = QColor(168, 85, 247);
    } else if (focus->kind == ObjectKind::Human || focus->level == AlertLevel::Suspicious) {
        durum = QStringLiteral("İNSAN");
        accent = QColor(240, 193, 74);
    } else if (focus->kind == ObjectKind::OtherAnimal || focus->level == AlertLevel::Abnormal) {
        durum = QStringLiteral("BAŞKA CANLI");
        accent = QColor(232, 93, 93);
    }

    p.setPen(Qt::NoPen);
    if (focus->named) {
        p.setBrush(QColor(36, 18, 48, 230));
    } else {
        p.setBrush(QColor(12, 18, 26, 210));
    }
    p.drawRoundedRect(card, 12, 12);
    p.setPen(QPen(focus->named ? QColor(168, 85, 247, 200) : QColor(80, 100, 120, 160), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(card, 12, 12);

    const QString head = focus->named && !focus->personName.isEmpty()
                             ? QStringLiteral("%1 — Detay").arg(focus->personName)
                             : (focus->label.isEmpty()
                                    ? QStringLiteral("ID %1 — Detay").arg(focus->id, 3, 10, QChar('0'))
                                    : QStringLiteral("%1 %2 — Detay")
                                          .arg(focus->label)
                                          .arg(focus->id, 3, 10, QChar('0')));
    p.setPen(QColor(255, 255, 255));
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(11);
    p.setFont(f);
    p.drawText(card.adjusted(12, 10, -12, 0), Qt::AlignLeft | Qt::AlignTop, head);

    f.setBold(false);
    f.setPointSize(9);
    p.setFont(f);
    p.setPen(QColor(180, 190, 200));
    const QString body = QStringLiteral("Kalıcı ID: %1\nDURUM: %2\nDETAY: %3")
                             .arg(focus->id, 3, 10, QChar('0'))
                             .arg(durum)
                             .arg(focus->named ? QStringLiteral("İsimli takip") : QStringLiteral("Canlı takip"));
    p.drawText(card.adjusted(12, 34, -12, 0), Qt::AlignLeft | Qt::AlignTop, body);

    p.setPen(accent);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(card.left() + 12, card.top() + 88, 200, 16), durum);

    drawMiniSpark(p, QRect(card.left() + 12, card.top() + 108, 110, 22),
                  focus->named ? QColor(168, 85, 247) : QColor(91, 155, 255), focus->id);
    drawMiniSpark(p, QRect(card.left() + 130, card.top() + 108, 110, 22),
                  focus->named ? QColor(196, 140, 255) : QColor(240, 140, 60), focus->id + 3);

    // 5s clip thumb
    const QRect clip(card.left() + 12, card.bottom() - 34, 70, 24);
    p.setBrush(QColor(20, 30, 40, 230));
    p.setPen(QPen(QColor(90, 110, 130), 1));
    p.drawRoundedRect(clip, 6, 6);
    p.setPen(QColor(230, 235, 240));
    p.drawText(clip, Qt::AlignCenter, QStringLiteral("▶ 5s"));
}

void VideoGLWidget::mousePressEvent(QMouseEvent *event)
{
    QMutexLocker lock(&m_mutex);
    if (m_tracks.isEmpty()) {
        return;
    }
    // cycle focus on click
    int idx = 0;
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].id == m_focusId) {
            idx = (i + 1) % m_tracks.size();
            break;
        }
    }
    m_focusId = m_tracks[idx].id;
    emit trackClicked(m_focusId);
    update();
    Q_UNUSED(event);
}

} // namespace farmeye
