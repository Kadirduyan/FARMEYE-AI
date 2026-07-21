#pragma once

#include <QString>
#include <QVector>
#include <QWidget>
#include <QImage>
#include <QHash>

class QGridLayout;
class QEvent;
class QLabel;

namespace farmeye {

struct CameraSlot {
    int id = 0;
    QString name;
    QString uri;
    bool enabled = false;
};

class CameraGridPanel : public QWidget {
    Q_OBJECT
public:
    explicit CameraGridPanel(QWidget *parent = nullptr);

    void setCameras(const QVector<CameraSlot> &cams);
public slots:
    void setThumbFrame(int camId, const QImage &frame);
    int activeId() const { return m_activeId; }

signals:
    void cameraSelected(int id, const QString &uri);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void rebuild();

    QGridLayout *m_grid = nullptr;
    QVector<CameraSlot> m_cams;
    int m_activeId = 0;
    QHash<int, QLabel *> m_thumbLabels;
};

} // namespace farmeye
