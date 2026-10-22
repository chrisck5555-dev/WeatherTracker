#pragma once

#include "Units.h"

#include <QString>
#include <optional>

// Turns stored (metric) values into display text. Display only: never write
// these strings back into the database.
namespace Format {

QString notRecorded();                                    // "Not recorded"
QString number(double v, int decimals);                   // 21.0 -> "21", 21.34 -> "21.3"
QString temperature(const std::optional<double> &celsius, Units::System s = Units::System::Metric);
QString windSpeed(const std::optional<double> &kmh, Units::System s = Units::System::Metric);
QString humidity(const std::optional<double> &pct);
QString pressure(const std::optional<double> &hpa, Units::System s = Units::System::Metric);
QString text(const QString &s);                           // empty -> "Not recorded"
QString precipitation(const QString &type, const std::optional<double> &mm,
                      Units::System s = Units::System::Metric);

} // namespace Format