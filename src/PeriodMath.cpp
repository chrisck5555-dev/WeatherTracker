#include "PeriodMath.h"

#include <algorithm>

namespace PeriodMath {

QDate startOf(const QDate &date, Period p)
{
    switch (p) {
    case Period::Day:
        return date;
    case Period::Week:
        return date.addDays(-(date.dayOfWeek() % 7));   // Mon=1..Sun=7, Sunday -> 0
    case Period::Month:
        return QDate(date.year(), date.month(), 1);
    case Period::Season: {
        const int m = date.month();
        if (m == 12) return QDate(date.year(), 12, 1);
        if (m <= 2)  return QDate(date.year() - 1, 12, 1);   // Jan/Feb: last December
        return QDate(date.year(), ((m - 3) / 3) * 3 + 3, 1); // 3, 6 or 9
    }
    case Period::Year:
        return QDate(date.year(), 1, 1);
    case Period::All:
        return QDate(1900, 1, 1);
    }
    return date;
}

QDate endOf(const QDate &date, Period p)
{
    const QDate s = startOf(date, p);
    switch (p) {
    case Period::Day:    return date;
    case Period::Week:   return s.addDays(6);
    case Period::Month:  return s.addMonths(1).addDays(-1);
    case Period::Season: return s.addMonths(3).addDays(-1);
    case Period::Year:   return s.addYears(1).addDays(-1);
    case Period::All:    return QDate(9999, 12, 31);
    }
    return date;
}

QDate step(const QDate &date, Period p, int direction)
{
    switch (p) {
    case Period::Day:    return date.addDays(direction);
    case Period::Week:   return date.addDays(7 * direction);
    case Period::Month:  return date.addMonths(direction);
    case Period::Season: return date.addMonths(3 * direction);
    case Period::Year:   return date.addYears(direction);
    case Period::All:    return date;
    }
    return date;
}

QString title(const QDate &date, Period p)
{
    const QDate s = startOf(date, p);
    switch (p) {
    case Period::Day:   return date.toString("dddd, MMMM d, yyyy");
    case Period::Week:  return "Week of " + s.toString("MMMM d, yyyy");
    case Period::Month: return date.toString("MMMM yyyy");
    case Period::Season:
        switch (s.month()) {
        case 12: return QString("Winter %1\u2013%2").arg(s.year())
                            .arg((s.year() + 1) % 100, 2, 10, QChar('0'));
        case 3:  return QString("Spring %1").arg(s.year());
        case 6:  return QString("Summer %1").arg(s.year());
        default: return QString("Fall %1").arg(s.year());
        }
    case Period::Year:  return date.toString("yyyy");
    case Period::All:   return "All time";
    }
    return QString();
}

QDate monthGridStart(int year, int month)
{
    const QDate first(year, month, 1);
    return first.addDays(-(first.dayOfWeek() % 7));
}

QDate withMonth(const QDate &date, int month)
{
    const int lastDay = QDate(date.year(), month, 1).daysInMonth();
    return QDate(date.year(), month, std::min(date.day(), lastDay));
}

EarlierDate sameDateInYear(const QDate &date, int year)
{
    if (date.month() == 2 && date.day() == 29) {
        if (QDate::isLeapYear(year))
            return {QDate(year, 2, 29), false};
        return {QDate(year, 3, 1), true};
    }
    return {QDate(year, date.month(), date.day()), false};
}

} // namespace PeriodMath