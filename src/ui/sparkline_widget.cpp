#include "sparkline_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace farmeye {

SparklineWidget::SparklineWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setMinimumSize(56, 22);
    setMaximumHeight(28);
    // seed mild wave so empty charts look like mockup
    for (int i = 0; i < m_maxPoints; ++i) {
        m_values.push_back(35.0 + 25.0 * ((i % 7) / 6.0));
    }
}

void SparklineWidget::setColor(const QColor &c)
{
    m_color = c;
    update();
}

void SparklineWidget::setFill(bool on)
{
    m_fill = on;
    update();
}

void SparklineWidget::setMaxPoints(int n)
{
    m_maxPoints = (std::max)(8, n);
}

void SparklineWidget::push(double value)
{
    m_values.push_back(value);
    while (m_values.size() > m_maxPoints) {
        m_values.removeFirst();
    }
    update();
}

void SparklineWidget::setValues(const QVector<double> &values)
{
    m_values = values;
    while (m_values.size() > m_maxPoints) {
        m_values.removeFirst();
    }
    update();
}

void SparklineWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_values.size() < 2) {
        return;
    }

    double lo = m_values[0], hi = m_values[0];
    for (double v : m_values) {
        lo = (std::min)(lo, v);
        hi = (std::max)(hi, v);
    }
    if (hi - lo < 1.0) {
        hi = lo + 1.0;
    }

    const QRectF r = rect().adjusted(1, 2, -1, -2);
    QPainterPath line;
    QPainterPath fill;
    for (int i = 0; i < m_values.size(); ++i) {
        const double t = static_cast<double>(i) / (m_values.size() - 1);
        const double x = r.left() + t * r.width();
        const double y = r.bottom() - ((m_values[i] - lo) / (hi - lo)) * r.height();
        if (i == 0) {
            line.moveTo(x, y);
            fill.moveTo(x, r.bottom());
            fill.lineTo(x, y);
        } else {
            line.lineTo(x, y);
            fill.lineTo(x, y);
        }
    }
    fill.lineTo(r.right(), r.bottom());
    fill.closeSubpath();

    if (m_fill) {
        QColor fc = m_color;
        fc.setAlpha(55);
        p.fillPath(fill, fc);
    }
    QPen pen(m_color, 1.6);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.drawPath(line);
}

} // namespace farmeye
