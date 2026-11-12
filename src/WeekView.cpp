#include "WeekView.h"

#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
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
constexpr int kRingRoom = 4;                    // Today's outline: 2 px, 2 px out (C5)
constexpr int kRowGap = Design::kGapButtons;    // 8 between rows
constexpr int kKeyGap = Design::kGapAreas;      // 12: rows to key
constexpr int kPad = 8;                         // S1 inside a row
constexpr int kLine = 28;                       // Body's line
constexpr int kLineGap = 4;                     // between a row's two lines
constexpr int kPicture = 24;
constexpr int kColumnGap = Design::kGapCards;   // 16 between columns (TB3: space, no lines)
constexpr int kLabelGap = Design::kGapButtons;  // 8: "High" to its number

QFont bodyFont(const QFont &base) { return tabularDigits(textFont(Design::TextStyle::Body, base)); }

QString dayText(const QDate &d) { return QLocale().toString(d, "dddd, MMMM d"); }

// The widest each column can be in any week, so the columns never move (T5).
double widestDay(const QFontMetricsF &fm)
{
    double w = 0;
    for (int month = 1; month <= 12; ++month)
        for (int day = 22; day <= 28; ++day)            // every weekday, in every month
            w = std::max(w, fm.horizontalAdvance(dayText(QDate(2026, month, day))));
    return w;
}

QString temperatureNumber(double c, Units::System s) { return Format::number(Units::cToDisplay(c, s), 1); }

// The number part right-aligned in its cell (T5), the words before it left-aligned.
void drawCell(QPainter &p, const QRectF &cell, const QString &words, const QString &value)
{
    p.drawText(cell, Qt::AlignLeft | Qt::AlignVCenter, words);
    p.drawText(cell, Qt::AlignRight | Qt::AlignVCenter, value);
}
} // namespace

WeekView::WeekView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent), m_state(state), m_repo(repo)
{
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible()) { reload(); update(); }
    });
    connect(&m_state, &AppState::todayChanged, this, qOverload<>(&QWidget::update));
    setFocusPolicy(Qt::TabFocus);                      // only Tab, not a mouse click
    setAccessibleDescription("Week. The Up and Down arrow keys move one day, Left and Right "
                             "change the week, and Enter opens the day.");
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    m_hover = new HoverFade(this, [this](const QPointF &pos) { return rowAt(pos); });
}

void WeekView::updateAccessibleText()
{
    const QDate d = m_state.currentDate();
    const auto it = m_weather.constFind(d);
    KeyboardNav::announce(this, Format::describeDay(
        d, it == m_weather.constEnd() ? std::nullopt : std::optional<WeatherDay>(*it),
        m_noteDates.contains(d), m_eventDates.contains(d), m_units));
}

bool WeekView::event(QEvent *event)
{
    // Left and Right stay with the window (previous and next week).
    if (KeyboardNav::claim(event, {Qt::Key_Up, Qt::Key_Down, Qt::Key_PageUp, Qt::Key_PageDown}))
        return true;
    return QWidget::event(event);
}

void WeekView::keyPressEvent(QKeyEvent *event)
{
    const QDate d = m_state.currentDate();
    switch (event->key()) {
    case Qt::Key_Up:       m_state.setCurrentDate(d.addDays(-1)); break;
    case Qt::Key_Down:     m_state.setCurrentDate(d.addDays(1)); break;
    case Qt::Key_PageUp:   m_state.setCurrentDate(d.addDays(-7)); break;
    case Qt::Key_PageDown: m_state.setCurrentDate(d.addDays(7)); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:    emit dayActivated(d); break;
    default:               QWidget::keyPressEvent(event); return;
    }
    event->accept();
}

void WeekView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    updateAccessibleText();
    update();
}

void WeekView::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
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

void WeekView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_hover->recheck();
}

void WeekView::reload()
{
    const Units::System units = Units::fromSetting(m_repo.setting("units", "metric"));
    if (units != m_units) {
        m_units = units;
        updateGeometry();                          // °F's numbers are wider
    }
    m_loadedStart = weekStart();
    const QDate end = m_loadedStart.addDays(6);
    m_weather = m_repo.weatherInRange(m_loadedStart, end);
    m_noteDates = m_repo.datesWithNotes(m_loadedStart, end);
    m_eventDates = m_repo.datesWithEvents(m_loadedStart, end);
    updateAccessibleText();
    update();
}

// ---------------- Geometry ----------------

WeekView::Columns WeekView::columns(bool two) const
{
    const QFontMetricsF fm(bodyFont(font()));
    const bool imperial = m_units == Units::System::Imperial;
    const double temp = fm.horizontalAdvance((imperial ? "-000.0 " : "-00.0 ") + Units::tempUnit(m_units));
    const double amount = fm.horizontalAdvance(imperial ? "00.00 in" : "000.0 mm");
    double type = 0;
    for (const char *t : {"None", "Rain", "Drizzle", "Snow", "Sleet", "Hail", "Freezing rain", "Mixed"})
        type = std::max(type, fm.horizontalAdvance(t));
    Columns c;
    c.cellHigh = fm.horizontalAdvance("High") + kLabelGap + temp;
    c.cellLow = fm.horizontalAdvance("Low") + kLabelGap + temp;
    c.cellPrecip = type + kLabelGap + amount;
    const double day = widestDay(fm);
    const double markers = CalendarTile::markersWidth(3);
    c.picture = day + kColumnGap;
    const double numbersStart = two ? 0 : c.picture + kPicture + kColumnGap;
    c.high = numbersStart;
    c.low = c.high + c.cellHigh + kColumnGap;
    c.precip = c.low + c.cellLow + kColumnGap;
    const double numbersEnd = c.precip + c.cellPrecip;
    c.markers = two ? c.picture + kPicture + kColumnGap : numbersEnd + kColumnGap;
    c.line1 = c.markers + markers;
    c.line2 = two ? numbersEnd : 0;
    return c;
}

bool WeekView::twoLines(int width) const
{
    const double inner = width - 2 * kRingRoom - 2 * kPad;
    return columns(false).line1 > inner;
}

int WeekView::rowHeight(bool two) const
{
    return 2 * kPad + kLine + (two ? kLineGap + kLine : 0);   // 44, or 76 (TB6)
}

int WeekView::keyHeight(int width) const
{
    return CalendarTile::keyHeight({}, tabularDigits(textFont(Design::TextStyle::Caption, font())),
                                   width - 2 * kRingRoom);
}

int WeekView::heightForWidth(int width) const
{
    return kRingRoom + 7 * rowHeight(twoLines(width)) + 6 * kRowGap + kKeyGap + keyHeight(width);
}

QSize WeekView::sizeHint() const
{
    const int w = int(std::ceil(columns(false).line1)) + 2 * kPad + 2 * kRingRoom;
    return QSize(w, heightForWidth(w));
}

QSize WeekView::minimumSizeHint() const
{
    const Columns c = columns(true);
    const int w = int(std::ceil(std::max(c.line1, c.line2))) + 2 * kPad + 2 * kRingRoom;
    // The height follows the width (heightForWidth); this is only the least it can be.
    return QSize(w, heightForWidth(sizeHint().width()));
}

QRectF WeekView::rowRect(int row) const
{
    // The rows share what the page gives them, at least their own height each.
    const double area = height() - kRingRoom - kKeyGap - keyHeight(width());
    const double h = std::max<double>(rowHeight(twoLines()), (area - 6 * kRowGap) / 7.0);
    const double y0 = std::round(kRingRoom + row * (h + kRowGap));
    const double y1 = std::round(kRingRoom + row * (h + kRowGap) + h);
    return QRectF(kRingRoom, y0, width() - 2 * kRingRoom, y1 - y0);
}

int WeekView::rowAt(const QPointF &pos) const
{
    for (int row = 0; row < 7; ++row)
        if (rowRect(row).contains(pos))
            return row;
    return -1;
}

// Press shows the row down; release over the same row activates it.
void WeekView::mousePressEvent(QMouseEvent *event)
{
    m_pressed = -1;
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = rowAt(event->position());
    update();
}

void WeekView::mouseReleaseEvent(QMouseEvent *event)
{
    const int row = m_pressed;
    m_pressed = -1;
    update();
    if (event->button() == Qt::LeftButton && row >= 0 && rowAt(event->position()) == row)
        emit dayActivated(m_loadedStart.addDays(row));
}

// ---------------- Painting ----------------

void WeekView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto colors = ThemeManager::instance()->colors();
    if (!Artwork::isClay())                       // clay: the page behind paints the pattern
        p.fillRect(rect(), colors.window);

    const bool two = twoLines();
    const Columns col = columns(two);
    const QFont body = bodyFont(font());
    const QDate today = QDate::currentDate();
    int todayRow = -1;

    for (int row = 0; row < 7; ++row) {
        const QDate date = m_loadedStart.addDays(row);
        if (date == today)
            todayRow = row;
        CalendarTile::State s;
        s.selected = date == m_state.currentDate();      // ST4
        s.focus = s.selected && hasFocus();              // F1
        s.pressed = row == m_pressed;
        s.hover = m_hover->amount(row);
        const QRectF shape = rowRect(row);
        const QColor text = CalendarTile::paint(p, shape, s);
        const QColor quiet = s.selected ? text : colors.mutedText;

        // The lines, centred in the row.
        const QRectF in = CalendarTile::contents(shape, s).adjusted(kPad, kPad, -kPad, -kPad);
        const double block = rowHeight(two) - 2 * kPad;
        const double top = std::round(in.top() + (in.height() - block) / 2);
        const QRectF line1(in.left(), top, in.width(), kLine);
        const QRectF line2(in.left(), top + kLine + kLineGap, in.width(), kLine);
        const QRectF numbers = two ? line2 : line1;
        auto cell = [&](double x, double w) { return QRectF(numbers.left() + x, numbers.top(), w, kLine); };

        p.setFont(s.selected ? semibold(body) : body);
        p.setPen(text);
        p.drawText(line1, Qt::AlignLeft | Qt::AlignVCenter, dayText(date));
        p.setFont(body);

        const auto it = m_weather.constFind(date);
        const bool hasWeather = it != m_weather.constEnd();
        if (hasWeather) {
            const QString picture = WeatherIcons::conditionName(*it);
            if (!picture.isEmpty())
                WeatherIcons::draw(p, picture, QPointF(line1.left() + col.picture,
                                                       line1.center().y() - kPicture / 2.0), kPicture);
            p.setPen(text);
            if (it->highC)
                drawCell(p, cell(col.high, col.cellHigh), "High",
                         temperatureNumber(*it->highC, m_units) + " " + Units::tempUnit(m_units));
            if (it->lowC)
                drawCell(p, cell(col.low, col.cellLow), "Low",
                         temperatureNumber(*it->lowC, m_units) + " " + Units::tempUnit(m_units));
            // The type at the left of its cell, the amount right-aligned: "Rain      4 mm".
            const QRectF precip = cell(col.precip, col.cellPrecip);
            if (it->precipType == "None") {
                drawCell(p, precip, "No precipitation", QString());
            } else if (!it->precipType.isEmpty() || it->precipMm) {
                const QString all = Format::precipitation(it->precipType, it->precipMm, m_units);
                const int comma = all.indexOf(", ");
                if (comma < 0)
                    drawCell(p, precip, it->precipMm ? QString() : all, it->precipMm ? all : QString());
                else
                    drawCell(p, precip, all.left(comma), all.mid(comma + 2));
            }
        } else {
            p.setPen(quiet);
            p.drawText(cell(col.high, col.cellHigh + col.cellLow + col.cellPrecip + 2 * kColumnGap),
                       Qt::AlignLeft | Qt::AlignVCenter, "No weather recorded");
        }
        CalendarTile::paintMarkers(p, hasWeather, m_noteDates.contains(date), m_eventDates.contains(date),
                                   line1.left() + col.markers, line1.center().y());
    }

    if (todayRow >= 0)
        CalendarTile::paintToday(p, rowRect(todayRow));    // after the rows (CAL1, C5)

    const int key = keyHeight(width());
    CalendarTile::paintKey(p, {}, tabularDigits(textFont(Design::TextStyle::Caption, font())),
                           QRectF(kRingRoom, height() - key, width() - 2 * kRingRoom, key));
}
