#pragma once

#include <QColor>
#include <QRectF>

class QPainter;

namespace ClayPaint {

// A puffy raised shape. `r` is the full area; the shape leaves room for its shadow.
void drawRaised(QPainter &p, const QRectF &r, double radius, const QColor &base, bool sunken);

// A flat rounded panel with a thin outline (not raised, no shadow).
void drawCard(QPainter &p, const QRectF &r, double radius, const QColor &fill);

} // namespace ClayPaint