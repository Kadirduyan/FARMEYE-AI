#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

namespace farmeye {

class SparklineWidget : public QWidget {
    Q_OBJECT
public:
    explicit SparklineWidget(QWidget *parent = nullptr);

    void setColor(const QColor &c);
    void setFill(bool on);
    void push(double value);
    void setValues(const QVector<double> &values);
    void setMaxPoints(int n);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color{0x3d, 0xd6, 0x8c};
    bool m_fill = true;
    int m_maxPoints = 28;
    QVector<double> m_values;
};

} // namespace farmeye
