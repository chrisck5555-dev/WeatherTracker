#include "LineChart.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

#include "ClayPaint.h"
#include "Format.h"

LineChart::LineChart(QWidget *parent) : QWidget(parent) {}

void LineChart::setData(const QString &title, const ChartMath::LineSeries &series,
                        const QList<QColor> &colors, Period period)
{
    m_title = title;
    m_data = series;
    m_colors = colors;
    m_period = period;
    update();
}

void LineChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    ClayPaint::drawCard(p, QRectF(rect()).adjusted(1, 1, -1, -1), 14, pal.color(QPalette::Base));

    QFont f = font();
    f.setPointSize(11);
    QFont title = f;
    title.setBold(true);
    title.setPointSize(13);
    const QFontMetrics fm(f);

    p.setPen(pal.color(QPalette::WindowText));
    p.setFont(title);
    p.drawText(QRectF(12, 6, width() - 24, 28), Qt::AlignVCenter | Qt::AlignLeft, m_title);
    p.setFont(f);

    const auto axis = ChartMath::lineAxis(m_data);
    if (!axis) {
        p.drawText(rect(), Qt::AlignCenter, "Nothing recorded for this period yet.");
        return;
    }

    // Legend (only when there is more than one line), top right
    const int count = int(m_data.values.size());
    if (count > 1) {
        double lx = width() - 16.0;
        for (int s = count - 1; s >= 0; --s) {
            const double textW = fm.horizontalAdvance(m_data.names[s]);
            lx -= textW;
            p.setPen(pal.color(QPalette::WindowText));
            p.drawText(QRectF(lx, 8, textW + 2, 24), Qt::AlignVCenter | Qt::AlignLeft,
                       m_data.names[s]);
            lx -= 38;
            p.setPen(QPen(m_colors.value(s), 4));
            p.drawLine(QPointF(lx, 20), QPointF(lx + 28, 20));
            lx -= 18;
        }
    }

    const QRectF plot(64, 44, width() - 64 - 16, height() - 44 - 38);
    const int n = m_data.days();
    auto yOf = [&](double v) {
        return plot.bottom() - (v - axis->min) / (axis->max - axis->min) * plot.height();
    };
    auto xOf = [&](int i) { return plot.left() + (i + 0.5) * plot.width() / n; };

    const int ticks = int(std::lround((axis->max - axis->min) / axis->step));
    for (int k = 0; k <= ticks; ++k) {
        const double v = axis->min + k * axis->step;
        const double y = yOf(v);
        p.setPen(QPen(pal.color(QPalette::Mid), 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(pal.color(QPalette::WindowText));
        p.drawText(QRectF(0, y - 12, plot.left() - 8, 24), Qt::AlignVCenter | Qt::AlignRight,
                   Format::number(v, 2));
    }

    // Each unbroken stretch is one line; a lone day becomes a dot. First series is drawn last.
    for (int s = count - 1; s >= 0; --s) {
        const QColor color = m_colors.value(s);
        const auto &values = m_data.values[s];
        p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(color);
        for (const QList<int> &run : ChartMath::runs(values)) {
            if (run.size() == 1) {
                p.drawEllipse(QPointF(xOf(run[0]), yOf(*values[run[0]])), 4.0, 4.0);
                continue;
            }
            QPolygonF line;
            for (int i : run)
                line << QPointF(xOf(i), yOf(*values[i]));
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(line);
            p.setBrush(color);
        }
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(pal.color(QPalette::Mid), 1));
    p.drawRect(plot);

    QList<int> where;
    QStringList text;
    for (int i = 0; i < n; ++i) {
        const QDate d = m_data.first.addDays(i);
        if (m_period == Period::Month) {
            where << i;
            text << QString::number(d.day());
        } else if (d.day() == 1) {
            where << i;
            text << d.toString("MMM");
        }
    }
    int widest = 0;
    for (const QString &t : text)
        widest = std::max(widest, fm.horizontalAdvance(t));
    p.setPen(pal.color(QPalette::WindowText));
    for (int k : ChartMath::thinnedIndices(int(where.size()), plot.width(), widest)) {
        const double x = xOf(where[k]);
        p.drawLine(QPointF(x, plot.bottom()), QPointF(x, plot.bottom() + 5));
        p.drawText(QRectF(x - 30, plot.bottom() + 6, 60, 24), Qt::AlignHCenter | Qt::AlignTop,
                   text[k]);
    }
}
