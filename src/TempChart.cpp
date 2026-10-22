#include "TempChart.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPolygonF>
#include <algorithm>

#include "ClayPaint.h"
#include "Format.h"
#include "ThemeManager.h"

namespace {

// Draw each unbroken stretch as one line; a lone day becomes a dot.
template <typename XOf, typename YOf>
void drawSeries(QPainter &p, const QList<std::optional<double>> &values,
                const QColor &color, XOf xOf, YOf yOf)
{
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

} // namespace

TempChart::TempChart(QWidget *parent) : QWidget(parent) {}

void TempChart::setData(const ChartMath::TempSeries &series, Period period, Units::System units)
{
    m_data = series;
    m_period = period;
    m_units = units;
    update();                                   // schedule a repaint; never paint directly
}

void TempChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    const auto theme = ThemeManager::instance()->colors();
    ClayPaint::drawCard(p, QRectF(rect()).adjusted(1, 1, -1, -1), 14, pal.color(QPalette::Base));

    QFont f = font();
    f.setPointSize(11);
    QFont title = f;
    title.setBold(true);
    title.setPointSize(13);
    const QFontMetrics fm(f);

    // --- Title and legend ---
    p.setPen(pal.color(QPalette::WindowText));
    p.setFont(title);
    p.drawText(QRectF(12, 6, width() - 24, 28), Qt::AlignVCenter | Qt::AlignLeft,
               "Temperature (" + Units::tempUnit(m_units) + ")");
    p.setFont(f);

    const auto axis = ChartMath::temperatureAxis(m_data);
    if (!axis) {
        p.drawText(rect(), Qt::AlignCenter, "Nothing recorded for this period yet.");
        return;
    }

    double lx = width() - 190.0;                // legend, top right
    for (const auto &[name, color] : {std::pair<QString, QColor>{"High", theme.high},
                                      std::pair<QString, QColor>{"Low", theme.low}}) {
        p.setPen(QPen(color, 4));
        p.drawLine(QPointF(lx, 20), QPointF(lx + 28, 20));
        p.setPen(pal.color(QPalette::WindowText));
        p.drawText(QRectF(lx + 34, 8, 50, 24), Qt::AlignVCenter | Qt::AlignLeft, name);
        lx += 90;
    }

    // --- Plot area and coordinate helpers ---
    const QRectF plot(64, 44, width() - 64 - 16, height() - 44 - 38);
    const int n = m_data.days();
    // Screen y grows DOWNWARD, so a bigger value must give a smaller y.
    auto yOf = [&](double v) {
        return plot.bottom() - (v - axis->min) / (axis->max - axis->min) * plot.height();
    };
    auto xOf = [&](int i) { return plot.left() + (i + 0.5) * plot.width() / n; };

    // --- Grid lines and y labels ---
    const int ticks = int(std::lround((axis->max - axis->min) / axis->step));
    for (int k = 0; k <= ticks; ++k) {          // integer loop: no accumulated rounding error
        const double v = axis->min + k * axis->step;
        const double y = yOf(v);
        p.setPen(QPen(pal.color(QPalette::Mid), 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(pal.color(QPalette::WindowText));
        p.drawText(QRectF(0, y - 12, plot.left() - 8, 24), Qt::AlignVCenter | Qt::AlignRight,
                   Format::number(v, 2));
    }

    // --- Freezing line: strongest text color, thick, bold label ---
    const double freezing = Units::cToDisplay(0.0, m_units);
    if (freezing >= axis->min && freezing <= axis->max) {
        p.setPen(QPen(pal.color(QPalette::WindowText), 3, Qt::DashLine));
        p.drawLine(QPointF(plot.left(), yOf(freezing)), QPointF(plot.right(), yOf(freezing)));
        QFont bold = f;
        bold.setBold(true);
        p.setFont(bold);
        p.drawText(QRectF(plot.left() + 6, yOf(freezing) - 24, 120, 22),
                   Qt::AlignVCenter | Qt::AlignLeft, "Freezing");
        p.setFont(f);
    }

    // --- The two lines (low first, so high is on top) ---
    drawSeries(p, m_data.low, theme.low, xOf, yOf);
    drawSeries(p, m_data.high, theme.high, xOf, yOf);

    // --- Frame and x labels ---
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(pal.color(QPalette::Mid), 1));
    p.drawRect(plot);

    QList<int> where;                           // day index of each candidate label
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