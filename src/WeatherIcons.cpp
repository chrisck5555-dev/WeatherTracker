#include "WeatherIcons.h"

#include <QCoreApplication>
#include <QHash>
#include <QPaintDevice>
#include <QPainter>

#include "Theme.h"
#include "ThemeManager.h"
#include "WeatherDay.h"
#include "WeatherInput.h"

namespace {

constexpr int kSourceSizes[] = {24, 48, 128};    // the sizes packed into the program

QString computeFolder()
{
    auto *themes = ThemeManager::instance();
    if (!themes)
        return QStringLiteral("clay-light");
    const bool dark = themes->colors().window.lightness() < 128;
    const bool clay = themes->style() == Theme::Style::Clay;
    return QString(clay ? "clay" : "contrast") + (dark ? "-dark" : "-light");
}

// Cheap to call in a paint loop: recomputed only when the theme changes.
const QString &folder()
{
    static QString current;
    static bool ready = false;
    if (!ready) {
        ready = true;
        current = computeFolder();
        if (auto *themes = ThemeManager::instance())
            QObject::connect(themes, &ThemeManager::themeChanged, [] { current = computeFolder(); });
    }
    return current;
}

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

} // namespace

namespace WeatherIcons {

QString precipName(const QString &t)
{
    if (t == "Rain")          return "rain";
    if (t == "Drizzle")       return "drizzle";
    if (t == "Snow")          return "snow";
    if (t == "Sleet")         return "sleet";
    if (t == "Freezing rain") return "freezing-rain";
    if (t == "Hail")          return "hail";
    return QString();         // "None" and "Mixed" have no matching picture
}

QString cloudName(const QString &c)
{
    if (c == "Clear" || c == "Mostly clear")       return "sunny";
    if (c == "Partly cloudy")                      return "partly-cloudy";
    if (c == "Mostly cloudy" || c == "Overcast")   return "cloudy";
    return QString();
}

QString windName(const QString &dir)
{
    if (!WeatherInput::windDirections().contains(dir))
        return QString();
    return "wind-from-" + dir.toLower();
}

QString conditionName(const WeatherDay &day)
{
    const QString precip = precipName(day.precipType);
    return precip.isEmpty() ? cloudName(day.cloud) : precip;
}

QString lookFolder() { return folder(); }

QString resourcePath(const QString &name, int size)
{
    return QString(":/weather/%1/%2/%3.png").arg(folder()).arg(size).arg(name);
}

QIcon icon(const QString &name)
{
    QIcon result;
    if (name.isEmpty())
        return result;
    for (int s : kSourceSizes)
        result.addFile(resourcePath(name, s));   // Qt picks the best size for the screen
    return result;
}

QIcon blankIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    return QIcon(pm);
}

QPixmap pixmap(const QString &name, int size, qreal dpr)
{
    if (name.isEmpty() || size <= 0)
        return QPixmap();
    const int px = qRound(size * dpr);
    const QString key = folder() + '/' + name + '/' + QString::number(px);
    auto &pixmaps = cache();
    if (const auto it = pixmaps.constFind(key); it != pixmaps.constEnd())
        return *it;

    int source = kSourceSizes[2];                // smallest packed size that is big enough
    for (int s : kSourceSizes)
        if (s >= px) { source = s; break; }
    QPixmap base(resourcePath(name, source));
    if (base.isNull())
        return QPixmap();
    QPixmap result = base.width() == px
        ? base : base.scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    result.setDevicePixelRatio(dpr);
    pixmaps.insert(key, result);
    return result;
}

void draw(QPainter &p, const QString &name, const QPointF &topLeft, int size)
{
    if (name.isEmpty())
        return;
    const QPixmap pm = pixmap(name, size, p.device()->devicePixelRatioF());
    if (!pm.isNull())
        p.drawPixmap(topLeft, pm);
}

} // namespace WeatherIcons
