#pragma once

#include "app/pipeline_controller.h"

#include <QVector>
#include <QWidget>

class QListWidget;
class QLabel;
class QStackedWidget;

namespace farmeye {

class AlertListPanel : public QWidget {
    Q_OBJECT
public:
    explicit AlertListPanel(QWidget *parent = nullptr);

public slots:
    void addAlert(const farmeye::AlertEvent &alert);
    void requestPlay(const farmeye::AlertEvent &alert) { emit playClipRequested(alert); }
    void requestName(const farmeye::AlertEvent &alert) { emit namePersonRequested(alert); }

signals:
    void playClipRequested(farmeye::AlertEvent alert);
    void namePersonRequested(farmeye::AlertEvent alert);

private:
    void rebuildList();

    QListWidget *m_list = nullptr;
    QLabel *m_empty = nullptr;
    QStackedWidget *m_stack = nullptr;
    QVector<AlertEvent> m_events;
};

} // namespace farmeye
