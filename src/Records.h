#pragma once

#include <QDate>
#include <QList>
#include <QMap>
#include <QString>

#include "Units.h"
#include "WeatherDay.h"

// Records, streaks and counts for a list of days. No GUI, so it is testable.
namespace Records {

struct Item {
    QString label;
    QString value;
    QDate date;               // valid = clicking the row opens this day
    bool header = false;      // a section heading (value is empty)
};

// `days` is the period's weather (metric). `today` ends streaks and decides whether
// a "current streak" row is shown. Empty list when no day has anything recorded.
QList<Item> build(const QMap<QDate, WeatherDay> &days, Units::System units, const QDate &today);

} // namespace Records
