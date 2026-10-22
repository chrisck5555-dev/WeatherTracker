#include "WeekView.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>

#include "AppState.h"
#include "Artwork.h"
#include "ClayPaint.h"
#include "Format.h"
#include "Markers.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "Theme.h"
#include "ThemeManager.h"
#include "WeatherIcons.h"

WeekView::WeekView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent), m_state(state), m_repo(repo)
{
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible()) { reload(); update(); }
    });
    connect(&m_state, &AppState::todayChanged, this, qOverload<>(&QWidget::update));
}

QDate WeekView::weekStart() const
{
    return PeriodMath::startOf(m_state.currentDate(), Period::Week);
}

void WeekView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();                      // data or units may have changed while hidden
}

void WeekView::reload()
{
    m_units = Units::fromSetting(m_repo.setting("units", "metric"));
    m_loadedStart = weekStart();
    const QDate end = m_loadedStart.addDays(6);
    m_weather = m_repo.weatherInRange(m_loadedStart, end);
    m_noteDates = m_repo.datesWithNotes(m_loadedStart, end);
    m_eventDates = m_repo.datesWithEvents(m_loadedStart, end);
    update();
}

QRectF WeekView::rowRect(int row) const
{
    const double h = (height() - 30) / 7.0;      // 30 px kept for the legend
    return QRectF(0, row * h, width(), h);
}

// Press shows the row down; release over the same row activates it.
void WeekView::mousePressEvent(QMouseEvent *event)
{
    m_pressed = -1;
    if (event->button() != Qt::LeftButton)
        return;
    for (int row = 0; row < 7; ++row)
        if (rowRect(row).contains(event->position()))
            m_pressed = row;
    update();
}

void WeekView::mouseReleaseEvent(QMouseEvent *event)
{
    const int row = m_pressed;
    m_pressed = -1;
    update();
    if (event->button() == Qt::LeftButton && row >= 0
        && rowRect(row).contains(event->position()))
        emit dayActivated(m_loadedStart.addDays(row));
}

void WeekView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    const bool clay = ThemeManager::instance()->style() == Theme::Style::Clay;
    if (!Artwork::isClay())                       // clay: the page behind paints the pattern
        p.fillRect(rect(), pal.color(QPalette::Window));

    QFont f = font();
    f.setPointSize(14);
    p.setFont(f);
    const QDate today = QDate::currentDate();

    for (int row = 0; row < 7; ++row) {
        const QDate date = m_loadedStart.addDays(row);
        const bool down = row == m_pressed;
        const QRectF shape = rowRect(row).adjusted(4, 3, -4, -3);
        const QRectF r = down ? shape.translated(0, 2) : shape;   // contents follow the press

        if (clay) {
            ClayPaint::drawRaised(p, shape, 14, pal.color(QPalette::Base), down);
        } else {
            QColor base = pal.color(QPalette::Base);
            if (down)
                base = base.darker(112);
            p.setPen(pal.color(QPalette::Mid));
            p.setBrush(base);
            p.drawRect(r);
        }

        p.setPen(pal.color(QPalette::Text));
        p.drawText(r.adjusted(12, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   date.toString("dddd, MMMM d"));

        // Summary text, right of the date
        QString summary = Format::notRecorded();
        const auto it = m_weather.constFind(date);
        if (it != m_weather.constEnd())
            summary = "High " + Format::temperature(it->highC, m_units)
                    + "   Low " + Format::temperature(it->lowC, m_units);
        else if (m_noteDates.contains(date) || m_eventDates.contains(date))
            summary = "No weather recorded";
        const QRectF summaryRect = r.adjusted(300, 0, -235, 0);   // room kept for the pictures
        p.drawText(summaryRect, Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(summary, Qt::ElideRight, int(summaryRect.width())));

        // Conditions and wind direction, in fixed slots so they line up between rows
        const int iconSize = int(qMin(36.0, r.height() - 14));
        if (it != m_weather.constEnd() && iconSize >= 16) {
            const double top = r.center().y() - iconSize / 2.0;
            WeatherIcons::draw(p, WeatherIcons::conditionName(*it), QPointF(r.right() - 215, top), iconSize);
            WeatherIcons::draw(p, WeatherIcons::windName(it->windDir), QPointF(r.right() - 170, top), iconSize);
        }

        // Markers on the right, in fixed slots so they line up between rows
        double x = r.right() - 90;
        const double y = r.center().y() - (clay ? 1 : 0);
        if (it != m_weather.constEnd())  drawMarker(p, Marker::Weather, QPointF(x, y));
        x += 28;
        if (m_noteDates.contains(date))  drawMarker(p, Marker::Note, QPointF(x, y));
        x += 28;
        if (m_eventDates.contains(date)) drawMarker(p, Marker::Event, QPointF(x, y));

        if (date == today) {
            p.setPen(QPen(pal.color(QPalette::Highlight), 3));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1.5, 1.5, -1.5, -1.5), clay ? 14 : 0, clay ? 14 : 0);
        }
    }

    // --- Legend ---
    QFont lf = font();
    lf.setPointSize(11);
    p.setFont(lf);
    p.setPen(QPen(pal.color(QPalette::Highlight), 3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(12, height() - 24, 22, 16), clay ? 6 : 0, clay ? 6 : 0);
    p.setPen(pal.color(QPalette::WindowText));
    p.drawText(QRectF(42, height() - 28, 200, 24), Qt::AlignVCenter | Qt::AlignLeft, "Today");
}