#pragma once

#include "Units.h"
#include "WeatherDay.h"

#include <QDate>
#include <QString>
#include <optional>

// Turns stored (metric) values into display text. Display only: never write
// these strings back into the database.
namespace Format {

QString notRecorded();                                    // "Not recorded"
QString number(double v, int decimals);                   // 21.0 -> "21", 21.34 -> "21.3"
QString temperature(const std::optional<double> &celsius, Units::System s = Units::System::Metric);
QString windSpeed(const std::optional<double> &kmh, Units::System s = Units::System::Metric);
// CF4: the side the wind blows toward, from the saved side it comes from: "NW" -> "SE".
QString windGoingTo(const QString &fromDir);
// "12 km/h going to the SE", "Going to the SE", "12 km/h", or "Not recorded".
QString wind(const std::optional<double> &kmh, const QString &fromDir,
             Units::System s = Units::System::Metric);
QString humidity(const std::optional<double> &pct);
QString pressure(const std::optional<double> &hpa, Units::System s = Units::System::Metric);
QString text(const QString &s);                           // empty -> "Not recorded"
QString precipitation(const QString &type, const std::optional<double> &mm,
                      Units::System s = Units::System::Metric);

// A day in one or two sentences, for screen readers:
// "Tuesday, October 27, 2026. Overcast, rain 4 mm, high 8.5 °C, low 1 °C. Has a note."
QString describeDay(const QDate &date, const std::optional<WeatherDay> &weather,
                    bool hasNote, bool hasEvent, Units::System s = Units::System::Metric);

} // namespace Format