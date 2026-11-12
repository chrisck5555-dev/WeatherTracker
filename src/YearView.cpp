#include "YearView.h"

#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

#include "AppState.h"
#include "Artwork.h"
#include "CalendarTile.h"
#include "ClayPaint.h"
#include "Design.h"
#include "Format.h"
#include "HoverFade.h"
#include "KeyboardNav.h"
#include "PeriodMath.h"
#include "Repository.h"
#include "Responsive.h"
#include "ThemeManager.h"
#include "Typography.h"

namespace {
constexpr int kRingRoom = 4;                    // Today's outline: 2 px, 2 px out (C5)
constexpr int kTileGap = Design::kGapButtons;   // 8 between tiles
constexpr int kKeyGap = Design::kGapAreas;      // 12: tiles to key
constexpr int kPad = 8;                         // S1 inside a tile
constexpr int kNameLine = 28;                   // Body's line: the month's name
constexpr int kCaptionLine = 20;                // Caption's line: the numbers
constexpr int kNameGap = 4;
constexpr int kContent = 2 * kPad + kNameLine + kNameGap + 3 * kCaptionLine;   // 108
constexpr int kMinPicture = 32;                 // a thinner strip wouldn't read as a picture
constexpr double kPictureShare = 0.4;           // IL1: at most 40% of the tile

QFont captionFont(const QFont &base) { return tabularDigits(textFont(Design::TextStyle::Caption, base)); }

QString seasonOf(int month)
{
    if (month == 12 || month <= 2) return "Winter";
    if (month <= 5) return "Spring";
    if (month <= 8) return "Summer";
    return "Fall";
}

QString daysRecorded(int n)
{
    if (n == 0)
        return "No days recorded";
    return n == 1 ? QStringLiteral("1 day recorded") : QString("%1 days recorded").arg(n);
}
} // namespace

YearView::YearView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent), m_state(state), m_repo(repo)
{
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible() && m_state.currentDate().year() != m_loadedYear)
            reload();
        updateAccessibleText();
        update();
    });
    connect(&m_state, &AppState::todayChanged, this, qOverload<>(&QWidget::update));
    setFocusPolicy(Qt::TabFocus);                      // only Tab, not a mouse click
    setAccessibleDescription("Year. The arrow keys choose a month, Page Up and Page Down "
                             "change the year, and Enter opens the month.");
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    m_hover = new HoverFade(this, [this](const QPointF &pos) { return tileAt(pos); });
}

void YearView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();                       // data or units may have changed while hidden
}

void YearView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_hover->recheck();
}

void YearView::reload()
{
    m_units = Units::fromSetting(m_repo.setting("units", "metric"));
    m_loadedYear = m_state.currentDate().year();
    const auto weather = m_repo.weatherInRange(QDate(m_loadedYear, 1, 1), QDate(m_loadedYear, 12, 31));
    QList<WeatherDay> byMonth[12];
    for (auto it = weather.constBegin(); it != weather.constEnd(); ++it)
        byMonth[it.key().month() - 1] << it.value();
    for (int m = 0; m < 12; ++m)
        m_months[m] = Stats::summarize(byMonth[m]);
    updateAccessibleText();
    update();
}

QString YearView::describe(int month) const
{
    const Stats::Summary &s = m_months[month - 1];
    QString out = QDate(m_state.currentDate().year(), month, 1).toString("MMMM yyyy") + ". "
                + daysRecorded(s.daysRecorded) + ".";
    if (s.high.value)
        out += " Average high " + Format::temperature(s.high.value, m_units) + ".";
    if (s.low.value)
        out += " Average low " + Format::temperature(s.low.value, m_units) + ".";
    return out;
}

void YearView::updateAccessibleText()
{
    KeyboardNav::announce(this, describe(m_state.currentDate().month()));
}

bool YearView::event(QEvent *event)
{
    if (KeyboardNav::claim(event, {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down,
                                   Qt::Key_PageUp, Qt::Key_PageDown}))
        return true;
    return QWidget::event(event);
}

void YearView::keyPressEvent(QKeyEvent *event)
{
    const QDate d = m_state.currentDate();
    const int cols = columns();
    switch (event->key()) {
    case Qt::Key_Left:     m_state.setCurrentDate(d.addMonths(-1)); break;
    case Qt::Key_Right:    m_state.setCurrentDate(d.addMonths(1)); break;
    case Qt::Key_Up:       m_state.setCurrentDate(d.addMonths(-cols)); break;   // a row up
    case Qt::Key_Down:     m_state.setCurrentDate(d.addMonths(cols)); break;
    case Qt::Key_PageUp:   m_state.setCurrentDate(d.addYears(-1)); break;
    case Qt::Key_PageDown: m_state.setCurrentDate(d.addYears(1)); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:    emit monthActivated(d); break;
    default:               QWidget::keyPressEvent(event); return;
    }
    event->accept();
}

void YearView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    updateAccessibleText();
    update();
}

void YearView::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

// ---------------- Geometry ----------------

int YearView::columns() const
{
    switch (Responsive::band(window()->width())) {     // RZ9
    case Responsive::Band::Wide:   return 4;
    case Responsive::Band::Medium: return 3;
    case Responsive::Band::Narrow: return 2;
    }
    return 4;
}

int YearView::minTileHeight() const
{
    return kContent + (Artwork::isClay() ? kMinPicture : 0);
}

int YearView::keyHeight(int width) const
{
    return CalendarTile::keyHeight({false, true}, captionFont(font()), width - 2 * kRingRoom);
}

int YearView::heightForWidth(int width) const
{
    const int rows = 12 / columns();
    return kRingRoom + rows * minTileHeight() + (rows - 1) * kTileGap + kKeyGap + keyHeight(width);
}

QSize YearView::sizeHint() const
{
    return QSize(1000, heightForWidth(1000));
}

QSize YearView::minimumSizeHint() const
{
    // Two tiles a row, each wide enough for its words ("September", "Average high -00.0 °F").
    const QFontMetricsF fm(captionFont(font()));
    const double line = fm.horizontalAdvance(QString::fromUtf8("Average high -000.0 °F"));
    const int tile = int(std::ceil(line + Design::kGapLabel)) + 2 * kPad;
    const int w = 2 * tile + kTileGap + 2 * kRingRoom;
    // The height follows the width (heightForWidth); this is only the least it can be.
    return QSize(w, heightForWidth(sizeHint().width()));
}

QRectF YearView::tileRect(int i) const
{
    const int cols = columns();
    const int rows = 12 / cols;
    const double area = height() - kRingRoom - kKeyGap - keyHeight(width());
    const double w = (width() - 2 * kRingRoom - (cols - 1) * kTileGap) / double(cols);
    const double h = std::max<double>(minTileHeight(), (area - (rows - 1) * kTileGap) / rows);
    const int row = i / cols, col = i % cols;
    const double x0 = std::round(kRingRoom + col * (w + kTileGap));
    const double x1 = std::round(kRingRoom + col * (w + kTileGap) + w);
    const double y0 = std::round(kRingRoom + row * (h + kTileGap));
    const double y1 = std::round(kRingRoom + row * (h + kTileGap) + h);
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

int YearView::tileAt(const QPointF &pos) const
{
    for (int i = 0; i < 12; ++i)
        if (tileRect(i).contains(pos))
            return i;
    return -1;
}

void YearView::mousePressEvent(QMouseEvent *event)
{
    m_pressed = -1;
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = tileAt(event->position());
    update();
}

void YearView::mouseReleaseEvent(QMouseEvent *event)
{
    const int i = m_pressed;
    m_pressed = -1;
    update();
    if (event->button() == Qt::LeftButton && i >= 0 && tileAt(event->position()) == i)
        emit monthActivated(PeriodMath::withMonth(m_state.currentDate(), i + 1));
}

// ---------------- Painting ----------------

void YearView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto c = ThemeManager::instance()->colors();
    if (!Artwork::isClay())                       // clay: the page behind paints the pattern
        p.fillRect(rect(), c.window);

    const QFont name = textFont(Design::TextStyle::Body, font());
    const QFont caption = captionFont(font());
    const QDate today = QDate::currentDate();
    const int year = m_state.currentDate().year();
    const double ow = ClayPaint::outlineWidth();
    int todayTile = -1;

    for (int i = 0; i < 12; ++i) {
        const int month = i + 1;
        if (year == today.year() && month == today.month())
            todayTile = i;
        CalendarTile::State s;
        s.selected = month == m_state.currentDate().month();     // ST4
        s.focus = s.selected && hasFocus();                       // F1
        s.pressed = i == m_pressed;
        s.hover = m_hover->amount(i);
        const QRectF tile = tileRect(i);
        const QColor text = CalendarTile::paint(p, tile, s);
        const QRectF in = CalendarTile::contents(tile, s);

        // The season's picture across the top, inside the outline (IL1, R5). No text on it.
        double top = in.top();
        const double picture = std::min(kPictureShare * tile.height(), tile.height() - kContent);
        if (Artwork::isClay() && picture >= kMinPicture) {
            const QRectF strip(tile.left() + ow, tile.top() + ow, tile.width() - 2 * ow, picture - ow);
            const QPixmap season = Artwork::seasonTile(month, strip.size().toSize(),
                                                       p.device()->devicePixelRatioF());
            if (!season.isNull()) {
                QPainterPath clip = ClayPaint::shape(tile.adjusted(ow, ow, -ow, -ow),
                                                     std::max(0.0, CalendarTile::radius() - ow));
                QPainterPath band;
                band.addRect(strip);
                p.save();
                p.setClipPath(clip.intersected(band));
                p.drawPixmap(strip.topLeft(), season);
                // The hover, press and focus tints cover the picture too.
                CalendarTile::State over = s;
                over.selected = false;
                over.muted = true;                        // tints only: no fill, no outline
                CalendarTile::paint(p, tile, over);
                p.restore();
                top = tile.top() + picture;
            }
        }

        const QRectF body = QRectF(in.left() + kPad, top + kPad, in.width() - 2 * kPad, 0);
        p.setPen(text);
        p.setFont(s.selected ? semibold(name) : name);
        const QRectF nameRow(body.left(), body.top(), body.width(), kNameLine);
        p.drawText(nameRow, Qt::AlignLeft | Qt::AlignVCenter, QLocale().monthName(month));
        if (Artwork::isClay() && picture >= kMinPicture) {
            p.setFont(caption);
            p.setPen(s.selected ? text : c.mutedText);
            p.drawText(nameRow, Qt::AlignRight | Qt::AlignVCenter, seasonOf(month));   // IL1
        }

        // The numbers: words at the left, values right-aligned (T5).
        const Stats::Summary &m = m_months[i];
        p.setFont(caption);
        double y = nameRow.bottom() + kNameGap;
        auto line = [&](const QString &words, const QString &value, const QColor &color) {
            const QRectF r(body.left(), y, body.width(), kCaptionLine);
            p.setPen(color);
            p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, words);
            if (!value.isEmpty())
                p.drawText(r, Qt::AlignRight | Qt::AlignVCenter, value);
            y += kCaptionLine;
        };
        if (m.daysRecorded == 0) {
            line(daysRecorded(0), QString(), s.selected ? text : c.mutedText);
            continue;
        }
        if (m.high.value)
            line("Average high", Format::temperature(m.high.value, m_units), text);
        if (m.low.value)
            line("Average low", Format::temperature(m.low.value, m_units), text);
        line(daysRecorded(m.daysRecorded), QString(), text);
    }

    if (todayTile >= 0)
        CalendarTile::paintToday(p, tileRect(todayTile));    // after the tiles (CAL1, C5)

    const int key = keyHeight(width());
    CalendarTile::paintKey(p, {false, true}, caption,
                           QRectF(kRingRoom, height() - key, width() - 2 * kRingRoom, key));
}
