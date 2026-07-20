#pragma once

#include "app/pipeline_controller.h"

#include <QDialog>
#include <QVector>
#include <QImage>

class QLabel;
class QTimer;

namespace farmeye {

class ClipPlayerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ClipPlayerDialog(const AlertEvent &alert, QWidget *parent = nullptr);

private:
    QLabel *m_view = nullptr;
    QLabel *m_info = nullptr;
    QTimer *m_timer = nullptr;
    QVector<QImage> m_frames;
    int m_index = 0;
};

} // namespace farmeye
