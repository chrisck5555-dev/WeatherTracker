#pragma once

#include <QPointF>

class QPainter;

enum class Marker { Weather, Note, Event };

// One small shape: circle = weather, square = note, triangle = event.
void drawMarker(QPainter &p, Marker kind, const QPointF &center);