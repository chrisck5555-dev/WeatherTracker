#pragma once

#include <QDate>
#include <QString>

#include "Period.h"

// Date/period calculations. No GUI, no database: pure functions, easy to test.
namespace PeriodMath {

// First and last day (inclusive) of the period containing `date`.
// Weeks start on Sunday; seasons are meteorological (Winter = Dec-Feb).
QDate startOf(const QDate &date, Period p);
QDate endOf(const QDate &date, Period p);

// Move one period earlier (direction = -1) or later (+1).
QDate step(const QDate &date, Period p, int direction);

// Heading text, e.g. "Week of October 4, 2026" or "Winter 2026-27".
QString title(const QDate &date, Period p);

// First cell (a Sunday) of the 6x7 month grid for the given month.
QDate monthGridStart(int year, int month);

// Same day-of-month in another month, clamped (Jan 31 -> Feb 28).
QDate withMonth(const QDate &date, int month);

// The date to compare against in an earlier year (Requirements §6).
struct EarlierDate {
    QDate date;
    bool usedMarch1;   // true when Feb 29 was replaced by March 1
};
EarlierDate sameDateInYear(const QDate &date, int year);

} // namespace PeriodMath