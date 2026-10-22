#pragma once

#include <QIcon>
#include <QPixmap>
#include <QPointF>
#include <QString>

class QPainter;
struct WeatherDay;

// Picks and draws the weather pictures for the current theme (clay/contrast x light/dark).
// Every picture sits beside text; none of them is the only way to read the weather.
namespace WeatherIcons {

// Icon names: "sunny", "rain", "wind-from-ne", ... An empty name means "no picture".
QString precipName(const QString &precipType);   // "Rain" -> "rain"; "None"/"Mixed" -> ""
QString cloudName(const QString &cloud);         // "Overcast" -> "cloudy"
QString windName(const QString &windDir);        // "NE" -> "wind-from-ne"
QString conditionName(const WeatherDay &day);    // precipitation first, else cloud cover

QString lookFolder();                                       // "clay-light", "contrast-dark", ...
QString resourcePath(const QString &name, int size);        // :/weather/<theme>/<size>/<name>.png
QIcon icon(const QString &name);                            // for combo boxes (all sizes)
QIcon blankIcon(int size);                                  // keeps text aligned in lists
QPixmap pixmap(const QString &name, int size, qreal dpr);   // cached, sharp at any scale
void draw(QPainter &p, const QString &name, const QPointF &topLeft, int size);

} // namespace WeatherIcons
