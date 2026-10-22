#include "WeatherInput.h"

#include <QLocale>
#include <cmath>

namespace WeatherInput {

Parse parseNumber(const QString &text, double &out)
{
    QString s = text.trimmed();
    if (s.isEmpty())
        return Parse::Blank;
    s.replace(',', '.');
    bool ok = false;
    const double v = s.toDouble(&ok);          // QString::toDouble ignores the PC's locale
    if (!ok || !std::isfinite(v))
        return Parse::Invalid;
    out = v;
    return Parse::Ok;
}

QString validateNumber(const QString &text, std::optional<double> min,
                       std::optional<double> max, std::optional<double> &value)
{
    value.reset();
    double v = 0;
    switch (parseNumber(text, v)) {
    case Parse::Blank:
        return QString();                      // blank = not recorded, always allowed
    case Parse::Invalid:
        return QStringLiteral("Please enter a number, like 12.5");
    case Parse::Ok:
        break;
    }
    if (min && v < *min) {
        return max ? QString("Must be between %1 and %2").arg(*min).arg(*max)
                   : QString("Must be %1 or more").arg(*min);
    }
    if (max && v > *max) {
        return min ? QString("Must be between %1 and %2").arg(*min).arg(*max)
                   : QString("Must be %1 or less").arg(*max);
    }
    value = v;
    return QString();
}

PrecipCheck checkPrecipPair(const QString &type, bool amountBlank)
{
    PrecipCheck c;
    if (type.isEmpty() && !amountBlank)
        c.typeError = QStringLiteral("Choose a precipitation type");
    else if (!type.isEmpty() && type != "None" && amountBlank)
        c.amountError = QStringLiteral("Enter an amount");
    return c;                                  // type "None": amount isn't used at all
}

std::optional<QDate> parseDate(const QString &text)
{
    QString s = text.simplified();
    s.remove(',');
    if (s.isEmpty())
        return std::nullopt;
    static const char *const formats[] = {
        "yyyy-M-d", "yyyy/M/d", "yyyy.M.d",
        "MMMM d yyyy", "MMM d yyyy", "d MMMM yyyy", "d MMM yyyy",
    };
    for (const char *format : formats) {
        // The C locale: English month names, whatever language the PC uses.
        const QDate d = QLocale::c().toDate(s, QString::fromLatin1(format));
        if (d.isValid() && d.year() >= 1900)
            return d;
    }
    return std::nullopt;
}

const QStringList &windDirections()
{
    static const QStringList l = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return l;
}

const QStringList &precipTypes()
{
    static const QStringList l = {"None", "Rain", "Drizzle", "Snow", "Sleet",
                                  "Freezing rain", "Hail", "Mixed"};
    return l;
}

const QStringList &cloudCovers()
{
    static const QStringList l = {"Clear", "Mostly clear", "Partly cloudy",
                                  "Mostly cloudy", "Overcast"};
    return l;
}

} // namespace WeatherInput