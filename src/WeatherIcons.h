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

// Icon names: "sunny", "rain", "wind-to-sw", ... An empty name means "no picture".
QString precipName(const QString &precipType);   // "Rain" -> "rain"; "None" -> "" (calendar)
QString choicePrecipName(const QString &type);   // the editor's cells: "None" -> "no-precipitation"
QString cloudName(const QString &cloud);         // "Overcast" -> "cloudy"
QString windName(const QString &fromDir);        // saved "NE" (from) -> "wind-to-sw"
QString windToName(const QString &toSide);       // "SW" -> "wind-to-sw": the arrow points SW
QString conditionName(const WeatherDay &day);    // precipitation first, else cloud cover

QString lookFolder();                                       // "clay-light", "contrast-dark", ...
QString resourcePath(const QString &name, int size);        // :/weather/<theme>/<size>/<name>.png
QIcon icon(const QString &name);                            // for combo boxes (all sizes)
QIcon blankIcon(int size);                                  // keeps text aligned in lists
QPixmap pixmap(const QString &name, int size, qreal dpr);   // cached, sharp at any scale
void draw(QPainter &p, const QString &name, const QPointF &topLeft, int size);

} // namespace WeatherIcons
