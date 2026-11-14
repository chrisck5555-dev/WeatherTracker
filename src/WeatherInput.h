#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <optional>

#include "Units.h"

// Turning typed text into numbers, and the editor's validation rules.
namespace WeatherInput {

enum class Parse { Blank, Ok, Invalid };

// Reads a typed number. Accepts "12.5", "12,5", spaces, a "°" and a typographic minus
// ("−12 °" is -12), K5. Rejects "abc", "inf", "nan".
Parse parseNumber(const QString &text, double &out);

// Returns "" when the text is fine, otherwise a plain-English message.
// `value` is set only when the text holds a valid number (unset when blank).
QString validateNumber(const QString &text, std::optional<double> min,
                       std::optional<double> max, std::optional<double> &value);

// Precipitation type and amount depend on each other (Requirements §4).
struct PrecipCheck {
    QString typeError;     // shown under the type dropdown
    QString amountError;   // shown under the amount box
};
PrecipCheck checkPrecipPair(const QString &type, bool amountBlank);

// Reads a typed date: "2026-10-21", "October 21, 2026", "Oct 21 2026", "21 October 2026".
// Slash dates like 10/11/2026 are refused, because day-first or month-first is a guess.
std::optional<QDate> parseDate(const QString &text);

// D54: the precipitation amount's unit follows its type: cm for snow and mm for everything
// else in metric, inches in imperial. Amounts are stored in mm.
struct AmountUnit {
    QString unit;                              // "mm", "cm", "in"
    QString spoken;                            // "millimetres", "centimetres", "inches"
};
AmountUnit precipUnit(const QString &type, Units::System s);
double amountToMm(double shown, const QString &type, Units::System s);
double mmToAmount(double mm, const QString &type, Units::System s);
// True when the same typed number means the same amount for both types (both snow or both not).
bool sameAmountUnit(const QString &typeA, const QString &typeB);

// CF4, D53: the editor's wind buttons name the side the wind blows toward; what is saved is
// the side it comes from (the weather convention, so 1.1.0 data stays as it is).
QString windFromGoingTo(const QString &goingTo);   // "SE" -> "NW"; "" -> ""
QString windGoingToFrom(const QString &from);      // "NW" -> "SE"; "" -> ""

// Narrator's words for a unit: "°C" -> "degrees Celsius", "km/h" -> "kilometres per hour".
QString spokenUnit(const QString &unit);

// The choices, in display order.
const QStringList &windDirections();
const QStringList &precipTypes();
const QStringList &cloudCovers();

} // namespace WeatherInput