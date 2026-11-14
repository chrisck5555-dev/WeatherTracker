#include "WeatherInput.h"

#include <QHash>
#include <QLocale>
#include <cmath>

#include "Format.h"

namespace WeatherInput {

Parse parseNumber(const QString &text, double &out)
{
    QString s = text;
    // K5: spaces of any kind and a degree sign are ignored; a comma is a decimal point and
    // a typographic minus (U+2212) is a minus.
    s.remove(QChar(0x00B0));
    s.replace(QChar(0x2212), QChar('-'));
    QString kept;
    for (QChar c : s)
        if (!c.isSpace())
            kept += c;
    s = kept;
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
        return QStringLiteral("Type a number, like 12.5");
    case Parse::Ok:
        break;
    }
    // FM5: plain words that say how to fix it.
    if ((min && v < *min) || (max && v > *max)) {
        if (min && max)
            return QString("Type a number from %1 to %2").arg(*min).arg(*max);
        return min ? QString("Type %1 or more").arg(*min) : QString("Type %1 or less").arg(*max);
    }
    value = v;
    return QString();
}

PrecipCheck checkPrecipPair(const QString &type, bool amountBlank)
{
    PrecipCheck c;
    if (type.isEmpty() && !amountBlank)
        c.typeError = QStringLiteral("Choose a type for this amount");
    else if (!type.isEmpty() && type != "None" && amountBlank)
        c.amountError = QStringLiteral("Type an amount, or choose None");
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

AmountUnit precipUnit(const QString &type, Units::System s)
{
    if (s == Units::System::Imperial)
        return {QStringLiteral("in"), QStringLiteral("inches")};
    if (type == QLatin1String("Snow"))
        return {QStringLiteral("cm"), QStringLiteral("centimetres")};
    return {QStringLiteral("mm"), QStringLiteral("millimetres")};
}

double amountToMm(double shown, const QString &type, Units::System s)
{
    return type == QLatin1String("Snow") ? Units::displayToSnowMm(shown, s) : Units::displayToMm(shown, s);
}

double mmToAmount(double mm, const QString &type, Units::System s)
{
    return type == QLatin1String("Snow") ? Units::snowMmToDisplay(mm, s) : Units::mmToDisplay(mm, s);
}

bool sameAmountUnit(const QString &a, const QString &b)
{
    return (a == QLatin1String("Snow")) == (b == QLatin1String("Snow"));
}

QString windFromGoingTo(const QString &goingTo) { return Format::windGoingTo(goingTo); }
QString windGoingToFrom(const QString &from) { return Format::windGoingTo(from); }

QString spokenUnit(const QString &unit)
{
    static const QHash<QString, QString> words = {
        {QStringLiteral("\u00B0C"), QStringLiteral("degrees Celsius")},
        {QStringLiteral("\u00B0F"), QStringLiteral("degrees Fahrenheit")},
        {QStringLiteral("km/h"), QStringLiteral("kilometres per hour")},
        {QStringLiteral("mph"), QStringLiteral("miles per hour")},
        {QStringLiteral("hPa"), QStringLiteral("hectopascals")},
        {QStringLiteral("inHg"), QStringLiteral("inches of mercury")},
        {QStringLiteral("%"), QStringLiteral("percent")},
        {QStringLiteral("mm"), QStringLiteral("millimetres")},
        {QStringLiteral("cm"), QStringLiteral("centimetres")},
        {QStringLiteral("in"), QStringLiteral("inches")},
    };
    return words.value(unit, unit);
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