#include "Artwork.h"

#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QPaintDevice>
#include <QPainter>

#include "WeatherIcons.h"

namespace {

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> pixmaps;
    static bool ready = false;
    if (!ready) {
        ready = true;   // pixmaps must be gone before the application object is
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                         [] { pixmaps.clear(); });
    }
    return pixmaps;
}

enum class Fit { Keep, Cover, Stretch };   // keep its shape / fill and crop / fill and stretch

// Loads `path` and scales it to `logical` size (times dpr).
QPixmap scaled(const QString &path, const QSize &logical, qreal dpr, Fit fit)
{
    if (logical.isEmpty())
        return QPixmap();
    const QSize px(qRound(logical.width() * dpr), qRound(logical.height() * dpr));
    const QString key = path + '|' + QString::number(px.width()) + 'x'
                        + QString::number(px.height()) + QChar('a' + int(fit));
    auto &pixmaps = cache();
    if (const auto it = pixmaps.constFind(key); it != pixmaps.constEnd())
        return *it;

    QPixmap base(path);
    if (base.isNull())
        return QPixmap();
    QPixmap result;
    if (fit == Fit::Stretch) {
        result = base.scaled(px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    } else if (fit == Fit::Cover) {
        const QPixmap big = base.scaled(px, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        result = big.copy((big.width() - px.width()) / 2, (big.height() - px.height()) / 2,
                          px.width(), px.height());
    } else if (base.size() == px) {
        result = base;
    } else {
        result = base.scaled(px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    result.setDevicePixelRatio(dpr);
    if (pixmaps.size() > 300)          // resizing the window creates many sizes
        pixmaps.clear();
    pixmaps.insert(key, result);
    return result;
}

QString look() { return WeatherIcons::lookFolder(); }

} // namespace

namespace Artwork {

bool isClay() { return look().startsWith("clay"); }

QPixmap marker(const QString &name, int size, qreal dpr)
{
    const int px = qRound(size * dpr);
    int source = 48;                               // smallest packed size that is big enough
    for (int s : {24, 32, 48})
        if (s >= px) { source = s; break; }
    return scaled(QString(":/markers/%1/%2/%3.png").arg(look()).arg(source).arg(name),
                  QSize(size, size), dpr, Fit::Keep);
}

QPixmap emptyDay(int size, qreal dpr)
{
    const int source = qRound(size * dpr) <= 128 ? 128 : 256;
    return scaled(QString(":/empty-day/%1/%2/empty-day.png").arg(look()).arg(source),
                  QSize(size, size), dpr, Fit::Keep);
}

bool bannerAvailable(const QString &name)
{
    return isClay() && QFile::exists(QString(":/backgrounds/%1/%2.png").arg(look(), name));
}

QPixmap banner(const QString &name, const QSize &size, qreal dpr)
{
    if (!isClay())
        return QPixmap();
    return scaled(QString(":/backgrounds/%1/%2.png").arg(look(), name), size, dpr, Fit::Stretch);
}

QPixmap seasonTile(int month, const QSize &size, qreal dpr)
{
    if (!isClay())
        return QPixmap();
    const char *season = "winter";                 // Dec, Jan, Feb
    if (month >= 3 && month <= 5)       season = "spring";
    else if (month >= 6 && month <= 8)  season = "summer";
    else if (month >= 9 && month <= 11) season = "autumn";
    return scaled(QString(":/backgrounds/%1/tile-%2.png").arg(look(), season), size, dpr, Fit::Cover);
}

void paintPage(QPainter &p, const QRectF &area, const QColor &fallback)
{
    if (isClay()) {
        constexpr int tileSize = 384;              // the 512 px picture, shown at this size
        const QPixmap tile = scaled(QString(":/backgrounds/%1/background-page.png").arg(look()),
                                    QSize(tileSize, tileSize), p.device()->devicePixelRatioF(), Fit::Keep);
        if (!tile.isNull()) {
            p.save();
            p.setClipRect(area);
            for (double y = area.top(); y < area.bottom(); y += tileSize)
                for (double x = area.left(); x < area.right(); x += tileSize)
                    p.drawPixmap(QPointF(x, y), tile);
            p.restore();
            return;
        }
    }
    p.fillRect(area, fallback);
}

} // namespace Artwork
