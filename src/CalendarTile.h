#pragma once

#include <QColor>
#include <QFont>
#include <QRectF>

class QPainter;

// The calendar's clickable tiles and rows (Month, Week, Year), drawn the same way everywhere
// (CAL1, K3): a control shape (R2) with its outline and raised edge (E1), and the states
//   - selected: accent fill (ST4)
//   - hover: the neutral tint (ST2), easing in and out (HoverFade)
//   - pressed: the press tint; the raised edge goes and the contents sink 1 px (ST3)
//   - keyboard focus: the accent tint (F1)
//   - outside the current month: muted text, no fill or outline
//   - today: a 2 px accent outline 2 px outside the tile (C5), drawn after all the tiles
namespace CalendarTile {

struct State {
    bool selected = false;
    bool pressed = false;
    bool focus = false;
    bool muted = false;
    double hover = 0;                   // 0..1
};

double radius();                        // R2: a control's 8, 4 in high contrast

// Draws the tile and returns the colour for the text on it.
QColor paint(QPainter &p, const QRectF &r, const State &state);

// Where the contents go: the tile, sunk with it while pressed.
QRectF contents(const QRectF &r, const State &state);

void paintToday(QPainter &p, const QRectF &r);

// The key under the calendar (K9), in Caption: the markers and the Today outline, each with
// its words. Items move to the next line when the width runs out.
struct Key {
    bool markers = true;                // weather, note and event markers
    bool today = true;
};
int keyHeight(const Key &key, const QFont &caption, double width);
void paintKey(QPainter &p, const Key &key, const QFont &caption, const QRectF &area);

// The markers a day has, in a row from `left`, centred on `centreY`. The pictures touch: each
// has its own clear margin, so the shapes still stand about 5 px apart.
inline constexpr int kMarkerSize = 18;
inline constexpr int kMarkerGap = 0;
int markersWidth(int count);
void paintMarkers(QPainter &p, bool weather, bool note, bool event, double left, double centreY);

} // namespace CalendarTile
