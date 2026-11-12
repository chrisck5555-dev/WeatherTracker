#include "MonthGridWidget.h"

#include <QHelpEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <algorithm>
#include <cmath>

#include "AppState.h"
#include "Artwork.h"
#include "CalendarTile.h"
#include "Design.h"
#include "Format.h"
#include "HoverFade.h"
#include "KeyboardNav.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "ThemeManager.h"
#include "Typography.h"
#include "WeatherIcons.h"

namespace {
// Down the widget: the weekday names, the tiles, the key.
constexpr int kNamesHeight = 20;                    // Caption's line
constexpr int kNamesGap = Design::kGapHeader;       // 8: names to tiles
constexpr int kKeyGap = Design::kGapAreas;          // 12: tiles to key
// Today's outline is 2 px wide, 2 px outside its tile (C5), so the tiles keep 4 px clear of
// the widget's left and right edges, where it would be cut off. Above and below, the gaps
// to the names and the key leave it room.
constexpr int kRingRoom = 4;
constexpr int kTileGap = Design::kGapButtons;       // 8 between tiles

// Inside a tile (CAL4; sizes include S1's 8 px padding).
constexpr int kPad = 8;
constexpr int kRowHeight = 28;                      // Body's line: the number and the picture
constexpr int kCaptionLine = 20;                    // Caption's line: high/low
constexpr int kPicture = 24;
constexpr int kPairGap = 4;                         // number to its picture
constexpr int kItemGap = Design::kGapButtons;       // 8: between the other items in a row
constexpr int kRowGap = 4;                          // between stacked rows
constexpr int kTwoRowsMinW = 158, kTwoRowsMinH = 64;
constexpr int kOneRowMinW = 132, kOneRowMinH = 44;

QFont numberFont(const QFont &base) { return tabularDigits(textFont(Design::TextStyle::Body, base)); }
QFont captionFont(const QFont &base) { return tabularDigits(textFont(Design::TextStyle::Caption, base)); }

// The widest the day number and high/low can be, so every month gets the same arrangement.
double numberWidth(const QFont &base) { return QFontMetricsF(numberFont(base)).horizontalAdvance("00"); }
double highLowWidth(const QFont &base)
{
    return QFontMetricsF(captionFont(base)).horizontalAdvance(QString::fromUtf8("-00° / -00°"));
}

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

int markerCount(bool weather, bool note, bool event) { return int(weather) + int(note) + int(event); }
} // namespace

MonthGridWidget::MonthGridWidget(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_repo(repo)
{
    connect(&m_state, &AppState::currentDateChanged, this, &MonthGridWidget::onDateChanged);
    connect(&m_state, &AppState::todayChanged, this, qOverload<>(&QWidget::update));
    // Only Tab gives it the keyboard: after a mouse click, Left/Right still mean
    // "previous/next month", as the help text says.
    setFocusPolicy(Qt::TabFocus);
    setAccessibleDescription("Month calendar. The arrow keys move one day, Page Up and "
                             "Page Down move one month, and Enter opens the day.");
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    m_hover = new HoverFade(this, [this](const QPointF &pos) { return hoverItemAt(pos); });
    reloadData();
}

int MonthGridWidget::hoverItemAt(const QPointF &pos) const
{
    const QDate d = dateAt(pos);
    return d.isValid() ? int(gridStart().daysTo(d)) : -1;
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
    updateAccessibleText();
    update();
}

void MonthGridWidget::onDateChanged()
{
    if (gridStart() != m_loadedStart)          // same month: reuse the cached markers
        reloadData();
    updateAccessibleText();
    update();                                  // the month shown may have changed
}

QString MonthGridWidget::describe(const QDate &d) const
{
    const auto it = m_weather.constFind(d);
    return Format::describeDay(
        d, it == m_weather.constEnd() ? std::nullopt : std::optional<WeatherDay>(*it),
        m_noteDates.contains(d), m_eventDates.contains(d),
        Units::fromSetting(m_repo.setting("units", "metric")));
}

void MonthGridWidget::updateAccessibleText()
{
    KeyboardNav::announce(this, describe(m_state.currentDate()));
}

// ---------------- Keyboard and tooltips ----------------

bool MonthGridWidget::event(QEvent *event)
{
    if (KeyboardNav::claim(event, {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down,
                                   Qt::Key_PageUp, Qt::Key_PageDown}))
        return true;
    if (event->type() == QEvent::ToolTip) {
        // CAL4: whatever a tile has no room for is still in its tooltip.
        auto *help = static_cast<QHelpEvent *>(event);
        const QDate d = dateAt(help->pos());
        if (d.isValid())
            QToolTip::showText(help->globalPos(), describe(d), this, tileRect(int(gridStart().daysTo(d))).toRect());
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}

void MonthGridWidget::keyPressEvent(QKeyEvent *event)
{
    const QDate d = m_state.currentDate();
    switch (event->key()) {
    case Qt::Key_Left:     m_state.setCurrentDate(d.addDays(-1)); break;
    case Qt::Key_Right:    m_state.setCurrentDate(d.addDays(1)); break;
    case Qt::Key_Up:       m_state.setCurrentDate(d.addDays(-7)); break;
    case Qt::Key_Down:     m_state.setCurrentDate(d.addDays(7)); break;
    case Qt::Key_PageUp:   m_state.setCurrentDate(d.addMonths(-1)); break;
    case Qt::Key_PageDown: m_state.setCurrentDate(d.addMonths(1)); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:    emit dayActivated(d); break;
    default:               QWidget::keyPressEvent(event); return;
    }
    event->accept();
}

void MonthGridWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    updateAccessibleText();
    update();                                  // the focus tint
}

void MonthGridWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

void MonthGridWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_hover->recheck();                        // the tiles moved under a still mouse
}

// ---------------- Geometry (shared by sizing, painting and clicking) ----------------

double MonthGridWidget::tileWidth(double width) const
{
    return std::max(0.0, (width - 2 * kRingRoom - 6 * kTileGap) / 7.0);
}

MonthGridWidget::TileLayout MonthGridWidget::tileLayout(double w, double h) const
{
    const double number = numberWidth(font());
    const double markers = CalendarTile::markersWidth(3);
    const double innerW = w - 2 * kPad;
    const double innerH = h - 2 * kPad;
    TileLayout t;
    // Two rows: the number and picture on top; high/low and the markers below.
    if (w >= kTwoRowsMinW && h >= kTwoRowsMinH && innerH >= kRowHeight + kCaptionLine
        && innerW >= number + kPairGap + kPicture
        && innerW >= highLowWidth(font()) + kItemGap + markers) {
        t.arrangement = Arrangement::TwoRows;
        t.highLow = true;
        return t;
    }
    // One row: the number, the picture, the markers.
    if (w >= kOneRowMinW && h >= kOneRowMinH && innerH >= kRowHeight
        && innerW >= number + kPairGap + kPicture + kItemGap + markers) {
        t.arrangement = Arrangement::OneRow;
        return t;
    }
    // Stacked: the number and picture on top, the markers below, wrapping. What still
    // doesn't fit drops from the end of CAL4's list: the picture, then the markers.
    t.arrangement = Arrangement::Stacked;
    t.picture = innerW >= number + kPairGap + kPicture;
    const int step = CalendarTile::kMarkerSize + CalendarTile::kMarkerGap;
    t.markersPerRow = std::clamp(int((innerW + CalendarTile::kMarkerGap) / step), 0, 3);
    return t;
}

double MonthGridWidget::minTileHeight(double w) const
{
    const TileLayout t = tileLayout(w, 1e6);
    if (t.arrangement != Arrangement::Stacked)
        return kOneRowMinH;                    // Two rows only when there's room to spare
    const int rows = t.markersPerRow > 0 ? (3 + t.markersPerRow - 1) / t.markersPerRow : 0;
    return 2 * kPad + kRowHeight + rows * (kRowGap + CalendarTile::kMarkerSize);
}

int MonthGridWidget::keyHeight(double width) const
{
    return CalendarTile::keyHeight({}, captionFont(font()), width - 2 * kRingRoom);
}

int MonthGridWidget::heightForWidth(int width) const
{
    const double tile = minTileHeight(tileWidth(width));
    return int(std::ceil(kNamesHeight + kNamesGap + 6 * tile + 5 * kTileGap + kKeyGap
                         + keyHeight(width)));
}

QSize MonthGridWidget::sizeHint() const
{
    return QSize(7 * 183 + 6 * kTileGap + 2 * kRingRoom, heightForWidth(1336));
}

QSize MonthGridWidget::minimumSizeHint() const
{
    // The narrowest at which every tile still has its number, picture and markers.
    const int w = 7 * int(std::ceil(numberWidth(font()) + kPairGap + kPicture + 2 * kPad))
                + 6 * kTileGap + 2 * kRingRoom;
    // The height follows the width (heightForWidth); this is only the least it can be.
    return QSize(w, heightForWidth(sizeHint().width()));
}

MonthGridWidget::Arrangement MonthGridWidget::arrangement() const
{
    const QRectF g = gridArea();
    return tileLayout(tileWidth(width()), (g.height() - 5 * kTileGap) / 6.0).arrangement;
}

QRectF MonthGridWidget::gridArea() const
{
    const double top = kNamesHeight + kNamesGap;
    const double bottom = height() - keyHeight(width()) - kKeyGap;
    return QRectF(kRingRoom, top, width() - 2 * kRingRoom, std::max(0.0, bottom - top));
}

QRectF MonthGridWidget::tileRect(int index) const
{
    const QRectF g = gridArea();
    const double w = tileWidth(width());
    const double h = std::max(0.0, (g.height() - 5 * kTileGap) / 6.0);
    const int row = index / 7, col = index % 7;
    // On whole pixels, so every outline is crisp; the rounding spreads over the gaps.
    const double x0 = std::round(g.left() + col * (w + kTileGap));
    const double x1 = std::round(g.left() + col * (w + kTileGap) + w);
    const double y0 = std::round(g.top() + row * (h + kTileGap));
    const double y1 = std::round(g.top() + row * (h + kTileGap) + h);
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

QDate MonthGridWidget::dateAt(const QPointF &pos) const
{
    for (int i = 0; i < 42; ++i)
        if (tileRect(i).contains(pos))
            return gridStart().addDays(i);
    return QDate();
}

// ---------------- Mouse: press shows the tile down, release activates ----------------

void MonthGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressDate = dateAt(event->position());
    update();
}

void MonthGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const QDate pressed = m_pressDate;
    m_pressDate = QDate();
    update();
    // Only counts if the release is over the tile that was pressed.
    if (pressed.isValid() && dateAt(event->position()) == pressed)
        emit dayActivated(pressed);
}

// ---------------- Painting ----------------

void MonthGridWidget::paintTile(QPainter &p, int index, const TileLayout &layout)
{
    const QDate date = gridStart().addDays(index);
    const bool inMonth = date.month() == m_state.currentDate().month();
    CalendarTile::State s;
    s.selected = date == m_state.currentDate();          // ST4
    s.focus = s.selected && hasFocus();                  // F1: only with the keyboard (Tab)
    s.pressed = date == m_pressDate;
    s.muted = !inMonth;
    s.hover = m_hover->amount(index);
    const QRectF tile = tileRect(index);
    const QColor textColor = CalendarTile::paint(p, tile, s);
    const QRectF in = CalendarTile::contents(tile, s).adjusted(kPad, kPad, -kPad, -kPad);

    const auto weather = m_weather.constFind(date);
    const bool hasWeather = weather != m_weather.constEnd();
    const bool hasNote = m_noteDates.contains(date);
    const bool hasEvent = m_eventDates.contains(date);

    // Top row: the number, and the picture right after it.
    QFont number = numberFont(font());
    if (s.selected)
        number = semibold(number);                       // as selected buttons (K3)
    p.setFont(number);
    p.setPen(textColor);
    const QRectF top(in.left(), in.top(), in.width(), kRowHeight);
    p.drawText(top, Qt::AlignLeft | Qt::AlignVCenter, QString::number(date.day()));
    const double pictureX = in.left() + numberWidth(font()) + kPairGap;
    if (layout.picture && hasWeather) {
        const QString name = WeatherIcons::conditionName(*weather);
        if (!name.isEmpty()) {
            p.setOpacity(inMonth ? 1.0 : 0.5);           // CAL1: muted beside the month
            WeatherIcons::draw(p, name, QPointF(pictureX, top.center().y() - kPicture / 2.0), kPicture);
            p.setOpacity(1.0);
        }
    }

    const int count = markerCount(hasWeather, hasNote, hasEvent);
    const double markerY = [&] {
        switch (layout.arrangement) {
        case Arrangement::TwoRows: return in.bottom() - kCaptionLine / 2.0;
        case Arrangement::OneRow:  return top.center().y();
        case Arrangement::Stacked: break;
        }
        return 0.0;
    }();

    switch (layout.arrangement) {
    case Arrangement::TwoRows: {
        // Below: high/low at the left, the markers at the right.
        if (layout.highLow && hasWeather) {
            p.setFont(captionFont(font()));
            p.setPen(textColor);
            p.drawText(QRectF(in.left(), in.bottom() - kCaptionLine, in.width(), kCaptionLine),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       highLow(*weather, Units::fromSetting(m_repo.setting("units", "metric"))));
        }
        CalendarTile::paintMarkers(p, hasWeather, hasNote, hasEvent,
                                   in.right() - CalendarTile::markersWidth(count), markerY);
        break;
    }
    case Arrangement::OneRow:
        CalendarTile::paintMarkers(p, hasWeather, hasNote, hasEvent,
                                   in.right() - CalendarTile::markersWidth(count), markerY);
        break;
    case Arrangement::Stacked: {
        if (layout.markersPerRow <= 0)
            break;
        // The markers fill the rows a tile with all three would use, from the top, so the
        // tiles line up.
        const QList<bool> has{hasWeather, hasNote, hasEvent};
        QList<int> kinds;
        for (int k = 0; k < 3; ++k)
            if (has[k])
                kinds << k;
        const int rows = (int(kinds.size()) + layout.markersPerRow - 1) / layout.markersPerRow;
        const int allRows = (3 + layout.markersPerRow - 1) / layout.markersPerRow;
        const double firstY = in.bottom() - allRows * CalendarTile::kMarkerSize
                            - (allRows - 1) * kRowGap + CalendarTile::kMarkerSize / 2.0;
        for (int r = 0; r < rows; ++r) {
            const auto part = kinds.mid(r * layout.markersPerRow, layout.markersPerRow);
            CalendarTile::paintMarkers(p, part.contains(0), part.contains(1), part.contains(2),
                                       in.left(), firstY + r * (CalendarTile::kMarkerSize + kRowGap));
        }
        break;
    }
    }
}

void MonthGridWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto c = ThemeManager::instance()->colors();
    if (!Artwork::isClay())                       // clay: the page behind paints the pattern
        p.fillRect(rect(), c.window);

    const QDate start = gridStart();
    const QRectF g = gridArea();
    const double tileH = std::max(0.0, (g.height() - 5 * kTileGap) / 6.0);
    const TileLayout layout = tileLayout(tileWidth(width()), tileH);

    // --- Weekday names (Sunday first) ---
    // CAL5: while the mouse is over a day, or the keyboard is on one (the grid only takes
    // the focus with Tab), that day's weekday name is Semibold, so its column stands out.
    const QFont names = textFont(Design::TextStyle::Caption, font());
    int markedCol = -1;
    if (m_hover->current() >= 0 && m_hover->current() < 42)
        markedCol = m_hover->current() % 7;
    else if (hasFocus())
        markedCol = m_state.currentDate().dayOfWeek() % 7;       // Sunday (7) is column 0
    p.setPen(c.text);
    for (int col = 0; col < 7; ++col) {
        const QRectF t = tileRect(col);
        p.setFont(col == markedCol ? semibold(names) : names);
        // QLocale numbers days Monday=1 ... Sunday=7, so column 0 (Sunday) is 7.
        p.drawText(QRectF(t.left(), 0, t.width(), kNamesHeight), Qt::AlignCenter,
                   QLocale().dayName(col == 0 ? 7 : col, QLocale::ShortFormat));
    }

    // --- The 42 tiles ---
    for (int i = 0; i < 42; ++i)
        paintTile(p, i, layout);

    // --- Today's outline, after the tiles so no neighbour covers it (CAL1, C5) ---
    const qint64 todayIndex = start.daysTo(QDate::currentDate());
    if (todayIndex >= 0 && todayIndex < 42)
        CalendarTile::paintToday(p, tileRect(int(todayIndex)));

    // --- The key (K9) ---
    CalendarTile::paintKey(p, {}, captionFont(font()),
                           QRectF(kRingRoom, height() - keyHeight(width()), width() - 2 * kRingRoom,
                                  keyHeight(width())));
}
