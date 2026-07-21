#pragma once

#include "app/pipeline_controller.h"

#include <QImage>
#include <QMutex>
#include <QOpenGLWidget>
#include <QVector>

class QMouseEvent;

namespace farmeye {

class VideoGLWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit VideoGLWidget(QWidget *parent = nullptr);

public slots:
    void setFrame(const QImage &frame, const QVector<farmeye::TrackOverlay> &tracks);
    void setImage(const QImage &frame);
    void setTracks(const QVector<farmeye::TrackOverlay> &tracks);

signals:
    void trackClicked(int trackId);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QImage m_frame;
    QVector<TrackOverlay> m_tracks;
    QMutex m_mutex;
    int m_focusId = -1;
};

} // namespace farmeye
