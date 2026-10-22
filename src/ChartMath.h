#pragma once

#include <QDate>
#include <QList>
#include <QMap>
#include <optional>
#include <QString>

#include "Units.h"
#include "WeatherDay.h"
#include "Period.h"

namespace ChartMath {

// An axis with round numbers: min, max and the gap between tick marks.
struct Axis {
    double min = 0, max = 1, step = 1;
};

// Smallest "nice" axis (steps of 1, 2 or 5 times a power of ten) covering lo..hi.
Axis niceAxis(double lo, double hi, int maxTicks = 6);

// Which of `count` labels to draw so they don't overlap. Returns indexes into the labels.
QList<int> thinnedIndices(int count, double availableWidth, double labelWidth, double gap = 12);

// Splits a series into unbroken stretches of days that have a value.
QList<QList<int>> runs(const QList<std::optional<double>> &values);

// One entry per day from `first`. Values are already in DISPLAY units.
struct TempSeries {
    QDate first;
    QList<std::optional<double>> high, low;
    int days() const { return int(high.size()); }
};

TempSeries temperatureSeries(const QDate &from, const QDate &to,
                             const QMap<QDate, WeatherDay> &days, Units::System units);

// unset when the series has no values at all
std::optional<Axis> temperatureAxis(const TempSeries &s);

// One bar per day (Month, Season) or per month (Year). Values are in DISPLAY units.
struct BarSeries {
    QList<double> values;
    QStringList labels;               // same length as values
    bool hasAnyData = false;          // any weather row in the period
    bool hasAnyBar = false;           // at least one value > 0
};

enum class PrecipKind { Rain, Snow };

BarSeries precipSeries(const QDate &from, const QDate &to, Period period,
                       const QMap<QDate, WeatherDay> &days, PrecipKind kind,
                       Units::System units);

// Axis for bars: always starts at 0.
Axis barAxis(const BarSeries &s);

// One or more lines over the same days (wind, pressure, humidity). Values are in DISPLAY units.
struct LineSeries {
    QDate first;
    QStringList names;                                    // one per line
    QList<QList<std::optional<double>>> values;           // one list per line, one entry per day
    bool zeroBased = false;                               // the axis starts at 0
    std::optional<Axis> fixedAxis;                        // set = use this axis as it is
    int days() const { return values.isEmpty() ? 0 : int(values.first().size()); }
};

enum class Metric { Wind, Pressure, Humidity };

LineSeries metricSeries(const QDate &from, const QDate &to,
                        const QMap<QDate, WeatherDay> &days, Metric metric, Units::System units);

// unset when no line has any value
std::optional<Axis> lineAxis(const LineSeries &s);

} // namespace ChartMath