#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <optional>

// Turning typed text into numbers, and the editor's validation rules.
namespace WeatherInput {

enum class Parse { Blank, Ok, Invalid };

// Reads a typed number. Accepts "12.5" and "12,5". Rejects "abc", "inf", "nan".
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

// The dropdown choices, in display order.
const QStringList &windDirections();
const QStringList &precipTypes();
const QStringList &cloudCovers();

} // namespace WeatherInput