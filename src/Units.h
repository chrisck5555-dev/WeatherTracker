#pragma once

#include <QString>

// All unit conversion lives here. Data is always stored metric;
// these functions are used only when showing or reading numbers.
namespace Units {

enum class System { Metric, Imperial };

System fromSetting(const QString &text);      // "imperial" -> Imperial, anything else -> Metric
QString toSetting(System s);

// Temperature (absolute) and temperature difference (delta)
double cToDisplay(double c, System s);
double displayToC(double shown, System s);
double deltaCToDisplay(double deltaC, System s);   // no +32!

// Wind, pressure, precipitation (stored mm), snow depth (stored mm)
double kmhToDisplay(double kmh, System s);
double displayToKmh(double shown, System s);
double hpaToDisplay(double hpa, System s);
double displayToHpa(double shown, System s);
double mmToDisplay(double mm, System s);            // rain: mm or inches
double displayToMm(double shown, System s);
double snowMmToDisplay(double mm, System s);        // snow: cm or inches
double displayToSnowMm(double shown, System s);

// Labels
QString tempUnit(System s);      // "°C" / "°F"
QString windUnit(System s);      // "km/h" / "mph"
QString pressureUnit(System s);  // "hPa" / "inHg"
QString rainUnit(System s);      // "mm" / "in"
QString snowUnit(System s);      // "cm" / "in"

} // namespace Units