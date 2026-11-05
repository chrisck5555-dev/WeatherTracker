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

// The keyboard focus, the same everywhere (buttons, tabs, check boxes, calendar days):
// a dashed line in the text colour following the rounded shape.
QPen focusPen();
// Draws it `inset` pixels inside `r` (calendar cells use 6, so the blue "today" outline
// stays visible).
void drawFocusRing(QPainter &p, const QRectF &r, double radius, double inset = 6);

} // namespace ClayPaint