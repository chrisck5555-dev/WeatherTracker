#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "WeatherDay.h"

// Aggregate numbers for a list of days. Metric only; conversion happens at display.
namespace Stats {

// An average and how many days it is based on.
struct Average {
    std::optional<double> value;   // unset when no day had data
    int count = 0;
};

struct PrecipRow {
    QString type;
    int days = 0;
    double totalMm = 0;
};

struct Summary {
    int daysRecorded = 0;          // days that have a weather row
    Average high, low, mean, wind, humidity, pressure;
    std::optional<double> precipTotalMm;     // unset when no day had a precip figure
    int precipDays = 0;                      // days that had a precip figure
    std::optional<double> precipPerDayMm;
    QString windDirection;                   // "N".."NW", "Variable", or "" for none
    QStringList cloudModes;                  // all tied most-common values
    QList<PrecipRow> precipByType;           // fixed display order, only types seen
};

// Meteorological compass: 8 points, clockwise from north.
int compassIndex(double degrees);                 // nearest point, ties clockwise
QString compassName(int index);
std::optional<double> compassDegrees(const QString &name);

// Circular mean of compass names. Returns "Variable" if the directions cancel out
// (mean resultant length below `threshold`), "" if the list is empty.
QString averageWindDirection(const QStringList &directions, double threshold = 0.25);

// Most common values; every value tied for first is returned, in first-seen order.
QStringList modes(const QStringList &values);

Summary summarize(const QList<WeatherDay> &days);

} // namespace Stats