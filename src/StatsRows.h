#pragma once

#include <QList>
#include <QString>

#include "Stats.h"
#include "Units.h"

// Turns a Stats::Summary into label/value text rows. No GUI, so it is testable.
namespace StatsRows {

struct Row {
    QString label;
    QString value;
    bool header = false;      // a section heading (value is empty)
};

// Empty list when no day in the period has anything recorded.
QList<Row> build(const Stats::Summary &s, int daysInPeriod, bool singleDay,
                 Units::System units);

} // namespace StatsRows