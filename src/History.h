#pragma once

#include <QDate>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

#include "Units.h"
#include "WeatherDay.h"
#include "Period.h"

// "On this day in earlier years". No GUI: returns text, easy to test.
namespace History {

constexpr int kWindowDays = 3;        // "usual" = this date +/- 3 days
constexpr int kMinWindowPoints = 5;   // values needed before "usual" is shown
constexpr int kMinYears = 2;          // earlier years needed for year-to-year stats

struct YearBlock {
    QString title;                    // "2024" or "2025 (March 1)"
    QStringList lines;
};

struct DayComparison {
    bool visible = false;             // false until an earlier year has data
    QString emptyMessage;             // set when no earlier year matches this date
    QStringList highlights;           // at most 4
    QList<YearBlock> blocks;          // newest first
};

// `earlier` = every recorded day BEFORE January 1 of date's year.
DayComparison compareDay(const QDate &date, const std::optional<WeatherDay> &thisDay,
                         const QMap<QDate, WeatherDay> &earlier, Units::System units
);

constexpr int kMinPeriodDays = 5;     // recorded days an earlier period needs to count
constexpr int kMinSidePoints = 5;     // values needed on EACH side of a comparison

struct PeriodComparison {
    bool visible = false;
    QString emptyMessage;             // set when no earlier period has enough days
    QStringList highlights;
    QList<YearBlock> blocks;          // newest first (same block type as the day panel)
};

// current = days in the viewed period. earlier = every recorded day BEFORE it.
PeriodComparison comparePeriod(const QDate &date, Period p,
                               const QMap<QDate, WeatherDay> &current,
                               const QMap<QDate, WeatherDay> &earlier,
                               Units::System units);

} // namespace History