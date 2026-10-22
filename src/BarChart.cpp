#include "BarChart.h"

#include <QFontMetrics>
#include <QPainter>
#include <algorithm>

#include "ClayPaint.h"
#include "Format.h"

BarChart::BarChart(QWidget *parent) : QWidget(parent) {}

void BarChart::setData(const QString &title, const ChartMath::BarSeries &series,
                       const QColor &color)
{
    m_title = title;
    m_data = series;
    m_color = color;
    update();
}

void BarChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    ClayPaint::drawCard(p, QRectF(rect()).adjusted(1, 1, -1, -1), 14, pal.color(QPalette::Base));

    QFont f = font();
    f.setPointSize(10);
    QFont title = f;
    title.setBold(true);
    title.setPointSize(12);
    const QFontMetrics fm(f);

    p.setPen(pal.color(QPalette::WindowText));
    p.setFont(title);
    p.drawText(QRectF(10, 4, width() - 20, 26), Qt::AlignVCenter | Qt::AlignLeft, m_title);
    p.setFont(f);

    if (!m_data.hasAnyData) {
        p.drawText(rect().adjusted(8, 30, -8, 0), Qt::AlignCenter | Qt::TextWordWrap,
                   "Nothing recorded for this period yet.");
        return;
    }

    const auto axis = ChartMath::barAxis(m_data);
    const QRectF plot(52, 36, width() - 52 - 10, height() - 36 - 30);
    const int n = int(m_data.values.size());
    auto yOf = [&](double v) {
        return plot.bottom() - (v - axis.min) / (axis.max - axis.min) * plot.height();
    };

    const int ticks = int(std::lround((axis.max - axis.min) / axis.step));
    for (int k = 0; k <= ticks; ++k) {
        const double v = axis.min + k * axis.step;
        const double y = yOf(v);
        p.setPen(QPen(pal.color(QPalette::Mid), 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(pal.color(QPalette::WindowText));
        p.drawText(QRectF(0, y - 12, plot.left() - 6, 24), Qt::AlignVCenter | Qt::AlignRight,
                   Format::number(v, 2));
    }

    // Bars: a little space between them, but never thinner than one pixel
    const double slot = plot.width() / n;
    const double barW = std::max(1.0, slot * 0.72);
    p.setPen(Qt::NoPen);
    p.setBrush(m_color);
    for (int i = 0; i < n; ++i) {
        const double v = m_data.values[i];
        if (v <= 0) continue;
        const double x = plot.left() + i * slot + (slot - barW) / 2;
        p.drawRect(QRectF(x, yOf(v), barW, plot.bottom() - yOf(v)));
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(pal.color(QPalette::Mid), 1));
    p.drawRect(plot);

    int widest = 0;
    for (const QString &t : m_data.labels)
        widest = std::max(widest, fm.horizontalAdvance(t));
    p.setPen(pal.color(QPalette::WindowText));
    for (int i : ChartMath::thinnedIndices(n, plot.width(), widest)) {
        const double x = plot.left() + (i + 0.5) * slot;
        p.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 4));
        p.drawText(QRectF(x - 24, plot.bottom() + 5, 48, 20), Qt::AlignHCenter | Qt::AlignTop,
                   m_data.labels[i]);
    }
}