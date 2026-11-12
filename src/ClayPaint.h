#pragma once

#include <QColor>
#include <QPainterPath>
#include <QRectF>

class QPainter;

namespace ClayPaint {

// A rectangle with "continuous" corners (a squircle): the curve starts earlier and eases
// into the straight edge, instead of a circle meeting the edge abruptly. `smoothing` 0 is
// an ordinary rounded rectangle; 0.6 is the iOS / Figma look.
QPainterPath continuousRect(const QRectF &r, double radius, double smoothing = 0.6);

// The shape every rounded thing in the program uses: continuous corners in the clay looks,
// ordinary corners in the high-contrast looks.
QPainterPath shape(const QRectF &r, double radius);

// A puffy raised shape. `r` is the full area; the shape leaves room for its shadow.
void drawRaised(QPainter &p, const QRectF &r, double radius, const QColor &base, bool sunken);

// A flat rounded panel with a thin outline (not raised, no shadow).
void drawCard(QPainter &p, const QRectF &r, double radius, const QColor &fill);

// R2: the corner radius of controls (8) and of cards and panels (16) in the current look;
// high contrast uses 4 for everything.
double controlRadius();
double cardRadius();
bool contrastLook();
int outlineWidth();                 // E2: 1 px, or 3 px in high contrast

// Fills `r`'s shape with a translucent overlay (hover, pressed, focus: ST2, ST3, F1).
void drawOverlay(QPainter &p, const QRectF &r, double radius, const QColor &overlay);

// E1, elevation 2: the raised edge Windows' own buttons have. In light looks the outline is a
// little darker along the bottom; in dark looks a little lighter along the top. `lift` fades
// it out as the control sinks (elevation 1). Not used in high contrast.
void drawRaisedEdge(QPainter &p, const QRectF &shapeRect, double radius, const QColor &edge,
                    double lift = 1.0);

// ST2's hover tint, scaled by `amount` (0 = none, 1 = full) as it eases in and out.
void drawHover(QPainter &p, const QRectF &r, double radius, double amount);

// The keyboard focus, the same everywhere (F1): a tint of the accent over the whole control.
// Draw it after the control's fill and BEFORE its text, so the text stays at full contrast.
// `onAccent` for accent-filled controls (Theme::focusOverlay).
void drawFocusTint(QPainter &p, const QRectF &r, double radius, bool onAccent = false);

} // namespace ClayPaint
