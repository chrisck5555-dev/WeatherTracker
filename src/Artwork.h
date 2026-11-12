#pragma once

#include <QPixmap>
#include <QSize>
#include <QString>

#include "BannerPlan.h"

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

// IL1: the Day view's banner, in the soft looks only (light: day; dark: night), built from
// the squares in resources/banner-squares (BannerPlan picks them from the date). Scaled to
// `size`'s height, never stretched, and cut off where the strip ends.
bool bannerAvailable();
QPixmap banner(const QDate &date, BannerPlan::Sky sky, bool snowGround, const QSize &size,
               qreal dpr);

// Season picture for a Year tile's strip (month 1-12). Fills `size` exactly, cut from the
// picture's bottom edge.
QPixmap seasonTile(int month, const QSize &size, qreal dpr);

// Repeating page background in clay looks; a plain fill in the others.
void paintPage(QPainter &p, const QRectF &area, const QColor &fallback);

} // namespace Artwork
