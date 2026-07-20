#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QEvent;

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
};

} // namespace farmeye
