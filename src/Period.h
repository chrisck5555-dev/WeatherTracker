#pragma once

#include <QString>

enum class Period { Day, Week, Month, Season, Year, All };   // All: Records tab only

inline QString periodName(Period p)          // "Day", "Week", ... for buttons
{
    switch (p) {
    case Period::Day:    return QStringLiteral("Day");
    case Period::Week:   return QStringLiteral("Week");
    case Period::Month:  return QStringLiteral("Month");
    case Period::Season: return QStringLiteral("Season");
    case Period::Year:   return QStringLiteral("Year");
    case Period::All:    return QStringLiteral("All time");
    }
    return QString();
}