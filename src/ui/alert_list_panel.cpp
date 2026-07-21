#include "alert_list_panel.h"
#include "sparkline_widget.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtGlobal>
#include <algorithm>

namespace farmeye {

namespace {

int sortRank(const AlertEvent &a)
{
    if (a.named) {
        return 0; // isimli kişiler en üstte (mor)
    }
    if (a.kind == ObjectKind::OtherAnimal || a.level == AlertLevel::Abnormal) {
        return 1;
    }
    if (a.kind == ObjectKind::Human || a.level == AlertLevel::Suspicious) {
        return 2;
    }
    return 3;
}

QWidget *makeAlertCard(const AlertEvent &alert, QWidget *parent, AlertListPanel *panel)
{
    QString accent = QStringLiteral("#3dd68c");
    QString bg = QStringLiteral("rgba(24, 42, 34, 230)");
    QString title = alert.title;

    if (alert.named) {
        accent = QStringLiteral("#a855f7");
        bg = QStringLiteral("rgba(42, 22, 58, 240)");
        if (title.isEmpty()) {
            title = alert.personName.isEmpty() ? QStringLiteral("İSİMLİ KİŞİ")
                                              : QStringLiteral("%1").arg(alert.personName.toUpper());
        }
    } else if (alert.kind == ObjectKind::Human || alert.level == AlertLevel::Suspicious) {
        accent = QStringLiteral("#f0c14a");
        bg = QStringLiteral("rgba(48, 38, 14, 235)");
        if (title.isEmpty()) {
            title = QStringLiteral("İNSAN ALGILANDI");
        }
    } else if (alert.kind == ObjectKind::OtherAnimal || alert.level == AlertLevel::Abnormal) {
        accent = QStringLiteral("#e85d5d");
        bg = QStringLiteral("rgba(52, 18, 22, 240)");
        if (title.isEmpty()) {
            title = QStringLiteral("BAŞKA CANLI");
        }
    } else if (title.isEmpty()) {
        title = QStringLiteral("İNEK");
    }

    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("alertCard"));
    card->setStyleSheet(QStringLiteral(
                            "QFrame#alertCard {"
                            "  background: %1;"
                            "  border: 1px solid %2;"
                            "  border-left: 5px solid %2;"
                            "  border-radius: 14px;"
                            "}")
                            .arg(bg, accent));

    const QString idText = alert.named && !alert.personName.isEmpty()
                               ? QStringLiteral("%1 · ID %2")
                                     .arg(alert.personName)
                                     .arg(alert.trackId, 3, 10, QChar('0'))
                               : QStringLiteral("ID: %1").arg(alert.trackId, 3, 10, QChar('0'));
    auto *idLab = new QLabel(idText, card);
    idLab->setStyleSheet(QStringLiteral("color:%1; font-weight:800; font-size:13px;").arg(accent));
    auto *timeLab = new QLabel(alert.timeText, card);
    timeLab->setStyleSheet(QStringLiteral("color:#9fb0c3; font-size:11px; font-weight:600;"));
    auto *titleLab = new QLabel(title, card);
    titleLab->setStyleSheet(QStringLiteral("color:#f4f7fb; font-weight:800; font-size:12px; letter-spacing:0.3px;"));
    auto *detailLab = new QLabel(alert.detail, card);
    detailLab->setStyleSheet(QStringLiteral("color:#a8b4c2; font-size:11px;"));
    detailLab->setWordWrap(true);

    auto *spark = new SparklineWidget(card);
    spark->setColor(QColor(accent));
    spark->setFixedHeight(24);
    QVector<double> wave;
    const int seed = alert.trackId * 17 + static_cast<int>(alert.kind) * 9;
    for (int i = 0; i < 24; ++i) {
        wave.push_back(18 + ((seed + i * 13) % 60));
    }
    spark->setValues(wave);

    auto *play = new QPushButton(QStringLiteral("▶  5s"), card);
    play->setObjectName(QStringLiteral("btnClip"));
    play->setCursor(Qt::PointingHandCursor);
    play->setFixedWidth(68);

    auto *top = new QHBoxLayout;
    top->addWidget(idLab);
    top->addStretch();
    top->addWidget(timeLab);

    auto *bot = new QHBoxLayout;
    bot->setSpacing(8);
    bot->addWidget(spark, 1);
    bot->addWidget(play);

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 11, 12, 11);
    lay->setSpacing(5);
    lay->addLayout(top);
    lay->addWidget(titleLab);
    lay->addWidget(detailLab);
    lay->addLayout(bot);

    if (alert.kind == ObjectKind::Human) {
        auto *nameBtn = new QPushButton(
            alert.named ? QStringLiteral("İsim değiştir") : QStringLiteral("İsim ver"), card);
        nameBtn->setObjectName(QStringLiteral("btnName"));
        nameBtn->setCursor(Qt::PointingHandCursor);
        nameBtn->setStyleSheet(QStringLiteral(
            "QPushButton#btnName {"
            "  background: rgba(168,85,247,40);"
            "  color: #e9d5ff;"
            "  border: 1px solid #a855f7;"
            "  border-radius: 8px;"
            "  padding: 4px 10px;"
            "  font-weight: 700;"
            "  font-size: 11px;"
            "}"
            "QPushButton#btnName:hover { background: rgba(168,85,247,70); }"));
        lay->addWidget(nameBtn);
        const AlertEvent copyName = alert;
        QObject::connect(nameBtn, &QPushButton::clicked, panel,
                         [panel, copyName]() { panel->requestName(copyName); });
    }

    const AlertEvent copy = alert;
    QObject::connect(play, &QPushButton::clicked, panel, [panel, copy]() { panel->requestPlay(copy); });
    return card;
}

QLabel *legend(const QString &text, const QString &bg, const QString &fg, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("legendChip"));
    l->setStyleSheet(QStringLiteral("background:%1; color:%2;").arg(bg, fg));
    return l;
}

} // namespace

AlertListPanel::AlertListPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("GELİŞMİŞ UYARI AKIŞI"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));

    auto *legendRow = new QHBoxLayout;
    legendRow->setSpacing(6);
    legendRow->addWidget(legend(QStringLiteral("● İSİMLİ"), QStringLiteral("rgba(55,25,70,220)"),
                                QStringLiteral("#e9d5ff"), this));
    legendRow->addWidget(legend(QStringLiteral("● BAŞKA CANLI"), QStringLiteral("rgba(80,25,30,220)"),
                                QStringLiteral("#ffb0b0"), this));
    legendRow->addWidget(legend(QStringLiteral("● İNSAN"), QStringLiteral("rgba(70,55,15,220)"),
                                QStringLiteral("#ffe08a"), this));
    legendRow->addWidget(legend(QStringLiteral("● İNEK"), QStringLiteral("rgba(25,55,40,220)"),
                                QStringLiteral("#9ef0c0"), this));
    legendRow->addStretch();

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("alertList"));
    m_list->setSpacing(8);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_empty = new QLabel(
        QStringLiteral("Henüz uyarı yok.\n\nBAŞLAT’a basın — tespitler burada listelenir.\n"
                       "İnsan kartında «İsim ver» ile kalıcı etiket atayın."),
        this);
    m_empty->setObjectName(QStringLiteral("emptyHint"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);

    m_stack = new QStackedWidget(this);
    auto *emptyPage = new QFrame(m_stack);
    emptyPage->setObjectName(QStringLiteral("alertShell"));
    auto *emptyLay = new QVBoxLayout(emptyPage);
    emptyLay->addStretch();
    emptyLay->addWidget(m_empty);
    emptyLay->addStretch();
    m_stack->addWidget(emptyPage);
    m_stack->addWidget(m_list);
    m_stack->setCurrentIndex(0);

    auto *shell = new QFrame(this);
    shell->setObjectName(QStringLiteral("alertShell"));
    auto *shellLay = new QVBoxLayout(shell);
    shellLay->setContentsMargins(8, 8, 8, 8);
    shellLay->addWidget(m_stack, 1);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);
    lay->addWidget(title);
    lay->addLayout(legendRow);
    lay->addWidget(shell, 1);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_events.size()) {
            emit playClipRequested(m_events[idx]);
        }
    });
}

void AlertListPanel::rebuildList()
{
    m_list->clear();
    if (m_events.isEmpty()) {
        m_stack->setCurrentIndex(0);
        return;
    }
    m_stack->setCurrentIndex(1);

    QVector<AlertEvent> sorted = m_events;
    std::stable_sort(sorted.begin(), sorted.end(), [](const AlertEvent &a, const AlertEvent &b) {
        return sortRank(a) < sortRank(b);
    });

    for (int i = 0; i < sorted.size(); ++i) {
        auto *item = new QListWidgetItem(m_list);
        auto *card = makeAlertCard(sorted[i], m_list, this);
        m_list->addItem(item);
        m_list->setItemWidget(item, card);
        item->setSizeHint(QSize(0, qMax(118, card->sizeHint().height() + 8)));
        int src = 0;
        for (int j = 0; j < m_events.size(); ++j) {
            if (m_events[j].trackId == sorted[i].trackId && m_events[j].timeText == sorted[i].timeText
                && m_events[j].title == sorted[i].title) {
                src = j;
                break;
            }
        }
        item->setData(Qt::UserRole, src);
    }
}

void AlertListPanel::addAlert(const AlertEvent &alert)
{
    m_events.push_front(alert);
    while (m_events.size() > 50) {
        m_events.removeLast();
    }
    rebuildList();
}

} // namespace farmeye
