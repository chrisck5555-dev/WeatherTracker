#pragma once

#include <QPixmap>
#include <QSize>
#include <QString>

class QColor;
class QPainter;
class QRectF;

// The pictures supplied by the designer: calendar markers, the empty-day picture,
// and the clay-only backgrounds. The right file is chosen from the current look.
namespace Artwork {

bool isClay();                                          // backgrounds exist for clay looks only

// "marker-weather", "marker-note" or "marker-event", drawn `size` pixels wide.
QPixmap marker(const QString &name, int size, qreal dpr);

QPixmap emptyDay(int size, qreal dpr);

bool bannerAvailable(const QString &name);              // clay look and the file is packed in

// Wide strip for the Day screen: banner-sunny/cloudy/rain/snow/plain. Fills `size` exactly.
QPixmap banner(const QString &name, const QSize &size, qreal dpr);

// Season picture for a month tile (month 1-12). Fills `size` exactly.
QPixmap seasonTile(int month, const QSize &size, qreal dpr);

// Repeating page background in clay looks; a plain fill in the others.
void paintPage(QPainter &p, const QRectF &area, const QColor &fallback);

} // namespace Artwork
