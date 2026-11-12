#include "CalendarTile.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStringList>
#include <algorithm>

#include "ClayPaint.h"
#include "Design.h"
#include "Markers.h"
#include "Theme.h"
#include "ThemeManager.h"

namespace CalendarTile {

double radius() { return ClayPaint::controlRadius(); }

QRectF contents(const QRectF &r, const State &state)
{
    // As ClayStyle's buttons: the shape stays and its contents sink 1 px (not in high contrast).
    return state.pressed && !ClayPaint::contrastLook() ? r.translated(0, 1) : r;
}

QColor paint(QPainter &p, const QRectF &r, const State &state)
{
    const auto c = ThemeManager::instance()->colors();
    const double rad = radius();
    const int ow = ClayPaint::outlineWidth();
    const QRectF shape = r;
    QColor fill, edge;
    if (state.selected) {
        fill = c.accent;
        edge = c.accent;
    } else if (state.muted) {
        fill = Qt::transparent;                    // CAL1: outside the month, no fill
        edge = Qt::transparent;
    } else {
        fill = c.surface;
        edge = c.border;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (fill.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(ClayPaint::shape(shape, rad));
    }
    const QColor under = fill.alpha() > 0 ? fill : c.window;
    if (state.pressed) {
        ClayPaint::drawOverlay(p, shape, rad, Theme::pressOverlay(c, under));    // ST3
    } else if (state.hover > 0.001) {
        QColor h = Theme::hoverOverlay(c, under);                                 // ST2, D58
        h.setAlphaF(float(h.alphaF() * std::min(state.hover, 1.0)));
        ClayPaint::drawOverlay(p, shape, rad, h);
    }
    if (state.focus)
        ClayPaint::drawFocusTint(p, shape, rad, state.selected);                 // F1
    if (edge.alpha() > 0) {
        const double half = ow / 2.0;
        p.setPen(QPen(edge, ow));
        p.setBrush(Qt::NoBrush);
        p.drawPath(ClayPaint::shape(shape.adjusted(half, half, -half, -half), std::max(0.0, rad - half)));
        if (!ClayPaint::contrastLook() && !state.pressed)
            ClayPaint::drawRaisedEdge(p, shape, rad, edge);                         // E1, ST3
    }
    p.restore();
    if (state.selected)
        return c.accentText;
    return state.muted ? c.mutedText : c.text;
}

void paintToday(QPainter &p, const QRectF &r)
{
    const auto c = ThemeManager::instance()->colors();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c.accent, 2));
    p.setBrush(Qt::NoBrush);
    // 2 px outside the tile, so its centre line is 3 px out; corners concentric (R3).
    p.drawPath(ClayPaint::shape(r.adjusted(-3, -3, 3, 3), radius() + 3));
    p.restore();
}

int markersWidth(int count)
{
    return count <= 0 ? 0 : count * kMarkerSize + (count - 1) * kMarkerGap;
}

void paintMarkers(QPainter &p, bool weather, bool note, bool event, double left, double centreY)
{
    double x = left + kMarkerSize / 2.0;
    auto one = [&](bool has, Marker m) {
        if (!has)
            return;
        drawMarker(p, m, QPointF(x, centreY));
        x += kMarkerSize + kMarkerGap;
    };
    one(weather, Marker::Weather);
    one(note, Marker::Note);
    one(event, Marker::Event);
}

namespace {

struct KeyItem {
    int kind;                           // 0 weather, 1 note, 2 event, 3 today
    QString text;
};

QList<KeyItem> keyItems(const Key &key)
{
    QList<KeyItem> items;
    if (key.markers)
        items << KeyItem{0, "Weather recorded"} << KeyItem{1, "Note"} << KeyItem{2, "Event"};
    if (key.today)
        items << KeyItem{3, "Today"};
    return items;
}

constexpr int kSampleW = 24;            // the marker or the Today outline's sample
constexpr int kLineH = 24;              // Caption's 20, plus room for the 18 px marker

// Lays the items out in lines; returns each item's x and line.
QList<QPointF> keyLayout(const QList<KeyItem> &items, const QFont &caption, double width, int *lines)
{
    const QFontMetrics fm(caption);
    QList<QPointF> at;
    double x = 0;
    int line = 0;
    for (const KeyItem &it : items) {
        const double w = kSampleW + Design::kGapButtons + fm.horizontalAdvance(it.text);
        if (x > 0 && x + w > width) {
            x = 0;
            ++line;
        }
        at << QPointF(x, line);
        x += w + Design::kGapAreas * 2;           // 24 between items (S1)
    }
    if (lines)
        *lines = items.isEmpty() ? 0 : line + 1;
    return at;
}

} // namespace

int keyHeight(const Key &key, const QFont &caption, double width)
{
    int lines = 0;
    keyLayout(keyItems(key), caption, width, &lines);
    return lines * kLineH + std::max(0, lines - 1) * Design::kGapButtons;
}

void paintKey(QPainter &p, const Key &key, const QFont &caption, const QRectF &area)
{
    const auto c = ThemeManager::instance()->colors();
    const QList<KeyItem> items = keyItems(key);
    const QList<QPointF> at = keyLayout(items, caption, area.width(), nullptr);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(caption);
    for (int i = 0; i < items.size(); ++i) {
        const double x = area.left() + at[i].x();
        const double y = area.top() + at[i].y() * (kLineH + Design::kGapButtons);
        const double cy = y + kLineH / 2.0;
        if (items[i].kind == 3) {
            p.setPen(QPen(c.accent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawPath(ClayPaint::shape(QRectF(x + 1, cy - 8, kSampleW - 2, 16),
                                        ClayPaint::contrastLook() ? Design::kRadiusContrast : 6));
        } else {
            drawMarker(p, static_cast<Marker>(items[i].kind), QPointF(x + kSampleW / 2.0, cy));
        }
        p.setPen(c.text);
        p.drawText(QRectF(x + kSampleW + Design::kGapButtons, y, 400, kLineH),
                   Qt::AlignVCenter | Qt::AlignLeft, items[i].text);
    }
    p.restore();
}

} // namespace CalendarTile
