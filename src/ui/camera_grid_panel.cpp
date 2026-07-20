#include "camera_grid_panel.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace farmeye {

CameraGridPanel::CameraGridPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("KAMERA SEÇİMİ"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));
    m_grid = new QGridLayout;
    m_grid->setSpacing(12);
    m_grid->setContentsMargins(0, 0, 0, 0);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);
    lay->addWidget(title);
    lay->addLayout(m_grid);
    lay->addStretch();
}

void CameraGridPanel::setCameras(const QVector<CameraSlot> &cams)
{
    m_cams = cams;
    while (m_cams.size() < 6) {
        CameraSlot s;
        s.id = m_cams.size();
        s.name = QStringLiteral("Camera %1").arg(s.id + 1);
        m_cams.push_back(s);
    }
    if (m_activeId < 0 || m_activeId >= m_cams.size()) {
        m_activeId = 0;
    }
    rebuild();
}

void CameraGridPanel::rebuild()
{
    QLayoutItem *child;
    while ((child = m_grid->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    for (int i = 0; i < 6; ++i) {
        const CameraSlot &c = m_cams[i];
        QString status = QStringLiteral("BOŞTA");
        QString accent = QStringLiteral("#4d5d70");
        QString badgeBg = QStringLiteral("rgba(40,50,62,220)");
        if (c.id == m_activeId && !c.uri.isEmpty()) {
            status = QStringLiteral("AKTİF");
            accent = QStringLiteral("#3dd68c");
            badgeBg = QStringLiteral("rgba(20,70,45,230)");
        } else if (c.enabled && !c.uri.isEmpty()) {
            status = QStringLiteral("KAYITTA");
            accent = QStringLiteral("#e85d5d");
            badgeBg = QStringLiteral("rgba(70,25,30,230)");
        }

        auto *tile = new QFrame(this);
        tile->setObjectName(QStringLiteral("camTile"));
        tile->setCursor(Qt::PointingHandCursor);
        tile->setMinimumSize(124, 118);
        tile->setProperty("camId", c.id);
        tile->setStyleSheet(QStringLiteral(
                                "QFrame#camTile {"
                                "  background: #0c1219;"
                                "  border: 2px solid %1;"
                                "  border-radius: 14px;"
                                "}"
                                "QFrame#camTile:hover { border-color: #8aa4c0; }")
                                .arg(accent));
        tile->installEventFilter(this);

        auto *thumb = new QFrame(tile);
        thumb->setMinimumHeight(68);
        static const char *grads[] = {
            "stop:0 #243044, stop:0.5 #152030, stop:1 #0c1820",
            "stop:0 #2a2838, stop:0.5 #181828, stop:1 #101018",
            "stop:0 #203428, stop:0.5 #14241c, stop:1 #0c1812",
            "stop:0 #283040, stop:0.5 #182028, stop:1 #101820",
            "stop:0 #302828, stop:0.5 #201818, stop:1 #140e0e",
            "stop:0 #223038, stop:0.5 #142028, stop:1 #0c161c",
        };
        thumb->setStyleSheet(QStringLiteral(
                                 "QFrame {"
                                 "  background: qlineargradient(x1:0,y1:0,x2:0.35,y2:1, %1);"
                                 "  border: none; border-radius: 10px;"
                                 "}")
                                 .arg(QLatin1String(grads[i % 6])));

        auto *overlay = new QLabel(QStringLiteral("CAM %1").arg(i + 1), thumb);
        overlay->setAlignment(Qt::AlignCenter);
        overlay->setStyleSheet(QStringLiteral("color: rgba(200,215,230,90); font-size:11px; font-weight:700; letter-spacing:2px; background:transparent;"));
        auto *thumbLay = new QVBoxLayout(thumb);
        thumbLay->setContentsMargins(0, 0, 0, 0);
        thumbLay->addStretch();
        thumbLay->addWidget(overlay);
        thumbLay->addStretch();

        auto *nameLab = new QLabel(c.name.isEmpty() ? QStringLiteral("Camera %1").arg(i + 1) : c.name, tile);
        nameLab->setStyleSheet(QStringLiteral("color:#e8eef5; font-size:12px; font-weight:700; background:transparent; border:none;"));

        auto *badge = new QLabel(QStringLiteral("●  %1").arg(status), tile);
        badge->setStyleSheet(QStringLiteral(
                                 "color:%1; font-size:10px; font-weight:800; background:%2;"
                                 "border-radius:8px; padding:3px 8px;")
                                 .arg(accent, badgeBg));

        auto *meta = new QHBoxLayout;
        meta->addWidget(nameLab);
        meta->addStretch();
        meta->addWidget(badge);

        auto *inner = new QVBoxLayout(tile);
        inner->setContentsMargins(8, 8, 8, 8);
        inner->setSpacing(6);
        inner->addWidget(thumb, 1);
        inner->addLayout(meta);
        m_grid->addWidget(tile, i / 2, i % 2);
    }
}

bool CameraGridPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *w = qobject_cast<QWidget *>(obj);
        if (w) {
            const int id = w->property("camId").toInt();
            if (id >= 0 && id < m_cams.size()) {
                m_activeId = id;
                rebuild();
                emit cameraSelected(id, m_cams[id].uri);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace farmeye
