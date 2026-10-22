#include "Markers.h"

#include <QPaintDevice>
#include <QPainter>
#include <QPolygonF>

#include "Artwork.h"
#include "ThemeManager.h"

void drawMarker(QPainter &p, Marker kind, const QPointF &c)
{
    // The designer's pictures first; the old plain shapes remain as a fallback.
    constexpr int size = 18;
    const char *name = nullptr;
    switch (kind) {
    case Marker::Weather: name = "marker-weather"; break;
    case Marker::Note:    name = "marker-note";    break;
    case Marker::Event:   name = "marker-event";   break;
    }
    const QPixmap picture = Artwork::marker(name, size, p.device()->devicePixelRatioF());
    if (!picture.isNull()) {
        p.drawPixmap(QPointF(c.x() - size / 2.0, c.y() - size / 2.0), picture);
        return;
    }

    const auto t = ThemeManager::instance()->colors();
    constexpr double r = 6;
    p.save();
    p.setPen(Qt::NoPen);
    switch (kind) {
    case Marker::Weather:
        p.setBrush(t.markWeather);
        p.drawEllipse(c, r, r);
        break;
    case Marker::Note:
        p.setBrush(t.markNote);
        p.drawRect(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
        break;
    case Marker::Event: {
        p.setBrush(t.markEvent);
        QPolygonF triangle;
        triangle << QPointF(c.x(), c.y() - r - 1)
                 << QPointF(c.x() + r + 1, c.y() + r)
                 << QPointF(c.x() - r - 1, c.y() + r);
        p.drawPolygon(triangle);
        break;
        }
    }
    p.restore();
}
