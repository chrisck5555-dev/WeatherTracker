#include "WeekStrip.h"

#include <QHelpEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <algorithm>
#include <cmath>

#include "CalendarTile.h"
#include "Design.h"
#include "Format.h"
#include "HoverFade.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "Responsive.h"
#include "Typography.h"
#include "WeatherIcons.h"

namespace {
constexpr int kRingRoom = 4;                    // Today's outline: 2 px, 2 px out (C5)
constexpr int kTileGap = Design::kGapButtons;   // 8 between tiles
constexpr int kPad = 8;                         // S1 inside a tile
constexpr int kNameLine = 28;                   // Body's line: "Sun 8"
constexpr int kPicture = 24;
constexpr int kCaptionLine = 20;                // Caption's line: high / low
constexpr int kMarkerLine = 20;
constexpr int kRowGap = 4;
constexpr int kTileHeight = 2 * kPad + kNameLine + kPicture + kCaptionLine + kMarkerLine + 3 * kRowGap;  // 120
constexpr int kMinTileWidth = 72;

QFont captionFont(const QFont &base) { return tabularDigits(textFont(Design::TextStyle::Caption, base)); }

// "8° / 1°"; one of them alone is named.
QString highLow(const WeatherDay &w, Units::System units)
{
    auto deg = [units](double c) {
        return Format::number(Units::cToDisplay(c, units), 0) + QString::fromUtf8("°");
    };
    if (w.highC && w.lowC)
        return deg(*w.highC) + " / " + deg(*w.lowC);
    if (w.highC)
        return "High " + deg(*w.highC);
    if (w.lowC)
        return "Low " + deg(*w.lowC);
    return QString();
}
} // namespace

WeekStrip::WeekStrip(Repository &repo, QWidget *parent)
    : QWidget(parent), m_repo(repo)
{
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName("This week");
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    m_hover = new HoverFade(this, [this](const QPointF &pos) { return tileAt(pos); });
}

void WeekStrip::setWeek(const QDate &viewed, Units::System units)
{
    m_viewed = viewed;
    m_units = units;
    m_start = PeriodMath::startOf(viewed, Period::Week);
    const QDate end = m_start.addDays(6);
    m_weather = m_repo.weatherInRange(m_start, end);
    m_noteDates = m_repo.datesWithNotes(m_start, end);
    m_eventDates = m_repo.datesWithEvents(m_start, end);
    update();
}

QString WeekStrip::describe(int i) const
{
    const QDate d = m_start.addDays(i);
    const auto it = m_weather.constFind(d);
    return Format::describeDay(d, it == m_weather.constEnd() ? std::nullopt : std::optional<WeatherDay>(*it),
                               m_noteDates.contains(d), m_eventDates.contains(d), m_units);
}

// ---------------- Geometry ----------------

int WeekStrip::columns() const
{
    return Responsive::band(window()->width()) == Responsive::Band::Narrow ? 4 : 7;
}

int WeekStrip::heightForWidth(int) const
{
    const int rows = columns() == 7 ? 1 : 2;
    return 2 * kRingRoom + rows * kTileHeight + (rows - 1) * kTileGap;
}

QSize WeekStrip::sizeHint() const
{
    const int w = 7 * 160 + 6 * kTileGap + 2 * kRingRoom;
    return QSize(w, heightForWidth(w));
}

QSize WeekStrip::minimumSizeHint() const
{
    const int w = 4 * kMinTileWidth + 3 * kTileGap + 2 * kRingRoom;
    return QSize(w, heightForWidth(w));
}

QRectF WeekStrip::tileRect(int i) const
{
    const int cols = columns();
    const double w = (width() - 2 * kRingRoom - (cols - 1) * kTileGap) / double(cols);
    const int row = i / cols, col = i % cols;
    const double x0 = std::round(kRingRoom + col * (w + kTileGap));
    const double x1 = std::round(kRingRoom + col * (w + kTileGap) + w);
    const double y0 = kRingRoom + row * (kTileHeight + kTileGap);
    return QRectF(x0, y0, x1 - x0, kTileHeight);
}

int WeekStrip::tileAt(const QPointF &pos) const
{
    for (int i = 0; i < 7; ++i)
        if (tileRect(i).contains(pos))
            return i;
    return -1;
}

// ---------------- Mouse and tooltips ----------------

void WeekStrip::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_hover->recheck();
}

bool WeekStrip::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int i = tileAt(help->pos());
        if (i >= 0)
            QToolTip::showText(help->globalPos(), describe(i), this, tileRect(i).toRect());
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}

void WeekStrip::mousePressEvent(QMouseEvent *event)
{
    m_pressed = event->button() == Qt::LeftButton ? tileAt(event->position()) : -1;
    update();
}

void WeekStrip::mouseReleaseEvent(QMouseEvent *event)
{
    const int i = m_pressed;
    m_pressed = -1;
    update();
    if (event->button() == Qt::LeftButton && i >= 0 && tileAt(event->position()) == i)
        emit dayActivated(m_start.addDays(i));
}

// ---------------- Painting ----------------

void WeekStrip::paintEvent(QPaintEvent *)
{
    if (!m_start.isValid())
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QFont name = textFont(Design::TextStyle::Body, font());
    const QFont caption = captionFont(font());
    const QDate today = QDate::currentDate();
    int todayTile = -1;

    for (int i = 0; i < 7; ++i) {
        const QDate date = m_start.addDays(i);
        if (date == today)
            todayTile = i;
        CalendarTile::State s;
        s.selected = date == m_viewed;                    // ST4: the day on the page
        s.pressed = i == m_pressed;
        s.hover = m_hover->amount(i);
        const QRectF tile = tileRect(i);
        const QColor text = CalendarTile::paint(p, tile, s);
        const QRectF in = CalendarTile::contents(tile, s).adjusted(kPad, kPad, -kPad, -kPad);

        // Down the tile, centred: "Sun 8", the picture, high / low, the markers.
        double y = in.top();
        p.setPen(text);
        p.setFont(s.selected ? semibold(name) : name);
        p.drawText(QRectF(in.left(), y, in.width(), kNameLine), Qt::AlignCenter,
                   QLocale().toString(date, "ddd d"));
        y += kNameLine + kRowGap;

        const auto it = m_weather.constFind(date);
        const bool hasWeather = it != m_weather.constEnd();
        if (hasWeather) {
            const QString picture = WeatherIcons::conditionName(*it);
            if (!picture.isEmpty())
                WeatherIcons::draw(p, picture, QPointF(in.center().x() - kPicture / 2.0, y), kPicture);
        }
        y += kPicture + kRowGap;

        if (hasWeather) {
            // Dropped, not cut, when the tile is too narrow (CAL4); the tooltip still has it.
            const QString hl = highLow(*it, m_units);
            if (QFontMetricsF(caption).horizontalAdvance(hl) <= in.width()) {
                p.setFont(caption);
                p.setPen(text);
                p.drawText(QRectF(in.left(), y, in.width(), kCaptionLine), Qt::AlignCenter, hl);
            }
        }
        y += kCaptionLine + kRowGap;

        const bool note = m_noteDates.contains(date), event = m_eventDates.contains(date);
        const int count = int(hasWeather) + int(note) + int(event);
        CalendarTile::paintMarkers(p, hasWeather, note, event,
                                   in.center().x() - CalendarTile::markersWidth(count) / 2.0,
                                   y + kMarkerLine / 2.0);
    }

    if (todayTile >= 0)
        CalendarTile::paintToday(p, tileRect(todayTile));    // after the tiles (CAL1, C5)
}
