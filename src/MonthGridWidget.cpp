#include "MonthGridWidget.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

#include "AppState.h"
#include "Artwork.h"
#include "ClayPaint.h"
#include "Markers.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "Theme.h"
#include "ThemeManager.h"
#include "WeatherIcons.h"

namespace {
constexpr int kHeaderHeight = 40;    // weekday names
constexpr int kLegendHeight = 44;    // marker explanations
constexpr int kWeekColWidth = 96;    // the "Open week" button column
}

MonthGridWidget::MonthGridWidget(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_repo(repo)
{
    connect(&m_state, &AppState::currentDateChanged, this, &MonthGridWidget::onDateChanged);
    connect(&m_state, &AppState::todayChanged, this, qOverload<>(&QWidget::update));
    reloadData();
}

// ---------------- Data ----------------

QDate MonthGridWidget::gridStart() const
{
    const QDate d = m_state.currentDate();
    return PeriodMath::monthGridStart(d.year(), d.month());
}

void MonthGridWidget::reloadData()
{
    m_loadedStart = gridStart();
    const QDate end = m_loadedStart.addDays(41);
    m_weather = m_repo.weatherInRange(m_loadedStart, end);   // 3 queries, not 42 x 3
    m_noteDates = m_repo.datesWithNotes(m_loadedStart, end);
    m_eventDates = m_repo.datesWithEvents(m_loadedStart, end);
    update();
}

void MonthGridWidget::onDateChanged()
{
    if (gridStart() != m_loadedStart)          // same month: reuse the cached markers
        reloadData();
    update();                                  // the month shown may have changed
}

// ---------------- Geometry (shared by painting and clicking) ----------------

QRectF MonthGridWidget::gridArea() const
{
    return QRectF(kWeekColWidth, kHeaderHeight,
                  width() - kWeekColWidth, height() - kHeaderHeight - kLegendHeight);
}

QRectF MonthGridWidget::weekButtonRect(int row) const
{
    const QRectF g = gridArea();
    const double ch = g.height() / 6.0;
    return QRectF(6, g.top() + row * ch + ch / 2 - 22, kWeekColWidth - 12, 44);
}

QDate MonthGridWidget::dateAt(const QPointF &pos) const
{
    const QRectF grid = gridArea();
    if (!grid.contains(pos))
        return QDate();
    const int col = std::min(6, int((pos.x() - grid.left()) / (grid.width() / 7.0)));
    const int row = std::min(5, int((pos.y() - grid.top()) / (grid.height() / 6.0)));
    return gridStart().addDays(row * 7 + col);
}

// ---------------- Mouse: press shows the button down, release activates ----------------

void MonthGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressWeek = -1;
    m_pressDate = QDate();
    for (int row = 0; row < 6; ++row)
        if (weekButtonRect(row).contains(event->position()))
            m_pressWeek = row;
    if (m_pressWeek < 0)
        m_pressDate = dateAt(event->position());
    update();
}

void MonthGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const int week = m_pressWeek;
    const QDate pressedDate = m_pressDate;
    m_pressWeek = -1;
    m_pressDate = QDate();
    update();

    // Only counts if the release is over the same button that was pressed.
    if (week >= 0 && weekButtonRect(week).contains(event->position()))
        emit weekActivated(gridStart().addDays(week * 7));
    else if (pressedDate.isValid() && dateAt(event->position()) == pressedDate)
        emit dayActivated(pressedDate);
}

// ---------------- Painting ----------------

void MonthGridWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QPalette pal = palette();
    const auto theme = ThemeManager::instance()->colors();
    const bool clay = ThemeManager::instance()->style() == Theme::Style::Clay;
    const double radius = clay ? 14.0 : 0.0;
    const double gap = clay ? 3.0 : 0.0;

    const QDate current = m_state.currentDate();
    const QDate today = QDate::currentDate();
    const QDate start = gridStart();
    const QRectF grid = gridArea();
    const double cellW = grid.width() / 7.0;
    const double cellH = grid.height() / 6.0;

    auto cellRect = [&](int row, int col) {
        return QRectF(grid.left() + col * cellW, grid.top() + row * cellH, cellW, cellH)
                   .adjusted(gap, gap, -gap, -gap);
    };

    if (!Artwork::isClay())                       // clay: the page behind paints the pattern
        p.fillRect(rect(), pal.color(QPalette::Window));

    // --- Weekday header (Sunday first) ---
    QFont headerFont = font();
    headerFont.setPointSize(12);
    headerFont.setBold(true);
    p.setFont(headerFont);
    p.setPen(pal.color(QPalette::WindowText));
    for (int col = 0; col < 7; ++col) {
        const QRectF r(grid.left() + col * cellW, 0, cellW, kHeaderHeight);
        // QLocale numbers days Monday=1 ... Sunday=7, so column 0 (Sunday) is 7.
        p.drawText(r, Qt::AlignCenter, QLocale().dayName(col == 0 ? 7 : col, QLocale::ShortFormat));
    }

    // --- The 42 cells ---
    QFont dayFont = font();
    dayFont.setPointSize(14);
    QFont legendFont = font();
    legendFont.setPointSize(11);
    p.setFont(dayFont);

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            const QDate date = start.addDays(row * 7 + col);
            const bool inMonth = date.month() == current.month();
            const bool down = date == m_pressDate;
            QRectF cell = cellRect(row, col);
            if (down)
                cell.translate(0, 2);                     // pressed: sink 2 px, no shadow

            if (clay && inMonth && !down) {               // soft shadow under the tile
                QColor sh = theme.shadow;
                sh.setAlpha(90);
                p.setPen(Qt::NoPen);
                p.setBrush(sh);
                p.drawRoundedRect(cell.translated(0, 3), radius, radius);
            }
            QColor base = inMonth ? pal.color(QPalette::Base) : pal.color(QPalette::Window);
            if (down)
                base = base.darker(112);
            p.setPen(QPen(pal.color(QPalette::Mid), 1));
            p.setBrush(base);
            p.drawRoundedRect(cell, radius, radius);

            p.setPen(pal.color(inMonth ? QPalette::Active : QPalette::Disabled, QPalette::Text));
            p.drawText(cell.adjusted(8, 6, -8, 0), Qt::AlignTop | Qt::AlignLeft,
                       QString::number(date.day()));

            // The day's conditions, top right (fainter on days of the neighbouring months).
            const auto weatherIt = m_weather.constFind(date);
            if (weatherIt != m_weather.constEnd()) {
                const int s = int(qBound(20.0, qMin(cell.height() * 0.45, cell.width() * 0.38), 40.0));
                p.setOpacity(inMonth ? 1.0 : 0.5);
                WeatherIcons::draw(p, WeatherIcons::conditionName(*weatherIt),
                                   QPointF(cell.right() - s - 6, cell.top() + 6), s);
                p.setOpacity(1.0);
            }

            // Markers: one symbol per kind present, along the bottom of the cell.
            double x = cell.left() + 16;
            const double y = cell.bottom() - 16;
            if (m_weather.contains(date))    { drawMarker(p, Marker::Weather, QPointF(x, y)); x += 22; }
            if (m_noteDates.contains(date))  { drawMarker(p, Marker::Note, QPointF(x, y)); x += 22; }
            if (m_eventDates.contains(date)) { drawMarker(p, Marker::Event, QPointF(x, y)); }
        }
    }

    // --- "Open week" buttons ---
    p.setFont(legendFont);
    for (int row = 0; row < 6; ++row) {
        const QRectF r = weekButtonRect(row);
        const bool down = row == m_pressWeek;
        if (clay) {
            ClayPaint::drawRaised(p, r.adjusted(0, -2, 0, 6), 14, pal.color(QPalette::Button), down);
        } else {
            p.setPen(QPen(pal.color(QPalette::Mid), 1));
            p.setBrush(down ? pal.color(QPalette::Mid) : pal.color(QPalette::Button));
            p.drawRoundedRect(r, 4, 4);
        }
        p.setPen(pal.color(QPalette::ButtonText));
        p.drawText(r.translated(0, down ? 2 : 0), Qt::AlignCenter, "Open week");
    }

    // --- Today's outline (drawn after the cells so neighbours can't overwrite it) ---
    const qint64 todayIndex = start.daysTo(today);
    if (todayIndex >= 0 && todayIndex < 42) {
        const int row = int(todayIndex / 7), col = int(todayIndex % 7);
        QRectF cell = cellRect(row, col);
        if (start.addDays(todayIndex) == m_pressDate)
            cell.translate(0, 2);
        p.setPen(QPen(pal.color(QPalette::Highlight), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(cell.adjusted(1.5, 1.5, -1.5, -1.5), radius, radius);
    }

    // --- Legend ---
    p.setFont(legendFont);
    p.setPen(pal.color(QPalette::WindowText));
    const double ly = height() - kLegendHeight / 2.0;
    double x = 12;
    auto legendItem = [&](Marker m, const QString &text) {
        drawMarker(p, m, QPointF(x + 6, ly));
        p.setPen(pal.color(QPalette::WindowText));
        p.drawText(QRectF(x + 20, ly - 14, 300, 28), Qt::AlignVCenter | Qt::AlignLeft, text);
        x += 20 + p.fontMetrics().horizontalAdvance(text) + 28;
    };
    legendItem(Marker::Weather, "Weather recorded");
    legendItem(Marker::Note, "Note");
    legendItem(Marker::Event, "Event");

    p.setPen(QPen(pal.color(QPalette::Highlight), 3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(x, ly - 9, 22, 18), clay ? 6 : 0, clay ? 6 : 0);
    p.setPen(pal.color(QPalette::WindowText));
    p.drawText(QRectF(x + 32, ly - 14, 300, 28), Qt::AlignVCenter | Qt::AlignLeft, "Today");
}