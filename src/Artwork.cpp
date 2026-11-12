#include "Artwork.h"

#include <QColor>
#include <QDate>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QPaintDevice>
#include <QPainter>
#include <algorithm>

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

enum class Fit { Keep, Cover, CoverBottom };   // keep its shape / fill and crop (from the bottom)

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
    if (fit == Fit::Cover || fit == Fit::CoverBottom) {
        const QPixmap big = base.scaled(px, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int top = fit == Fit::Cover ? (big.height() - px.height()) / 2 : big.height() - px.height();
        result = big.copy((big.width() - px.width()) / 2, top, px.width(), px.height());
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

bool bannerAvailable()
{
    return isClay() && QFile::exists(QStringLiteral(":/banner/skies/sky_day_clear.png"));
}

namespace {

// One sky or ground picture of the banner, loaded once.
const QImage &bannerPart(const QString &path)
{
    static QHash<QString, QImage> parts;
    auto it = parts.find(path);
    if (it == parts.end())
        it = parts.insert(path, QImage(path).convertToFormat(QImage::Format_ARGB32_Premultiplied));
    return *it;
}

} // namespace

QPixmap banner(const QDate &date, BannerPlan::Sky sky, bool snowGround, const QSize &size,
               qreal dpr)
{
    if (!bannerAvailable() || size.isEmpty() || !date.isValid())
        return QPixmap();
    const QSize px(qRound(size.width() * dpr), qRound(size.height() * dpr));
    const QString key = QString("banner|%1|%2|%3|%4|%5x%6").arg(look(), date.toString(Qt::ISODate))
                            .arg(int(sky)).arg(int(snowGround)).arg(px.width()).arg(px.height());
    auto &pixmaps = cache();
    if (const auto it = pixmaps.constFind(key); it != pixmaps.constEnd())
        return *it;

    // The squares are drawn at their own size (162 px) side by side, then the whole row is
    // scaled to the strip's height at once, so no seams appear between them. Never stretched:
    // each square stays square, and the last one is cut off where the strip ends.
    constexpr int kSquare = 162, kPiece = kSquare / 3;
    const bool night = look() == QLatin1String("clay-dark");
    const QString time = night ? QStringLiteral("night") : QStringLiteral("day");
    const QString surface = snowGround ? QStringLiteral("snow") : QStringLiteral("grass");
    const int count = std::max(1, (px.width() + px.height() - 1) / std::max(1, px.height()));
    QImage row(count * kSquare, kSquare, QImage::Format_ARGB32_Premultiplied);
    row.fill(Qt::transparent);
    {
        QPainter p(&row);
        const QList<BannerPlan::Square> squares = BannerPlan::plan(date, sky, count);
        for (int i = 0; i < squares.size(); ++i) {
            QString skyName = squares[i].sky;
            if (night)
                skyName.replace(QLatin1String("sun"), QLatin1String("moon"));
            p.drawImage(i * kSquare, 0,
                        bannerPart(QString(":/banner/skies/sky_%1_%2.png").arg(time, skyName)));
            for (int j = 0; j < 3; ++j) {
                const BannerPlan::Piece &g = squares[i].ground[size_t(j)];
                p.drawImage(i * kSquare + j * kPiece, 0,
                            bannerPart(QString(":/banner/ground/ground_%1_%2_%3_%4_%5.png")
                                           .arg(time, surface).arg(g.h0).arg(g.h1)
                                           .arg(g.concaveUp ? "concave-up" : "concave-down")));
            }
        }
    }
    const QImage scaledRow = row.scaledToHeight(px.height(), Qt::SmoothTransformation);
    QPixmap result = QPixmap::fromImage(scaledRow.copy(0, 0, std::min(px.width(), scaledRow.width()),
                                                       px.height()));
    result.setDevicePixelRatio(dpr);
    if (pixmaps.size() > 300)
        pixmaps.clear();
    pixmaps.insert(key, result);
    return result;
}

QPixmap seasonTile(int month, const QSize &size, qreal dpr)
{
    if (!isClay())
        return QPixmap();
    const char *season = "winter";                 // Dec, Jan, Feb
    if (month >= 3 && month <= 5)       season = "spring";
    else if (month >= 6 && month <= 8)  season = "summer";
    else if (month >= 9 && month <= 11) season = "autumn";
    // The pictures' seasons are along their edges, so a strip shows the bottom edge, where
    // the snow, flowers, sunflowers and leaves are.
    return scaled(QString(":/backgrounds/%1/tile-%2.png").arg(look(), season), size, dpr,
                  Fit::CoverBottom);
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
