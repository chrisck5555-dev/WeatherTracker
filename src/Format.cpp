#include "Format.h"

namespace Format {

QString notRecorded() { return QStringLiteral("Not recorded"); }

QString number(double v, int decimals)
{
    QString s = QString::number(v, 'f', decimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))       // -0.04 rounds to "-0": show "0"
        s = QStringLiteral("0");
    return s;
}

namespace {
QString withUnit(double v, int decimals, const QString &unit)
{
    return number(v, decimals) + " " + unit;
}
} // namespace

QString temperature(const std::optional<double> &c, Units::System s)
{
    return c ? withUnit(Units::cToDisplay(*c, s), 1, Units::tempUnit(s)) : notRecorded();
}
QString windSpeed(const std::optional<double> &k, Units::System s)
{
    return k ? withUnit(Units::kmhToDisplay(*k, s), 1, Units::windUnit(s)) : notRecorded();
}
QString humidity(const std::optional<double> &p)
{
    return p ? withUnit(*p, 0, "%") : notRecorded();
}
QString pressure(const std::optional<double> &h, Units::System s)
{
    if (!h) return notRecorded();
    const bool imp = s == Units::System::Imperial;
    return withUnit(Units::hpaToDisplay(*h, s), imp ? 2 : 1, Units::pressureUnit(s));
}

QString text(const QString &s)
{
    return s.isEmpty() ? notRecorded() : s;
}

QString precipitation(const QString &type, const std::optional<double> &mm, Units::System s)
{
    if (type.isEmpty() && !mm)
        return notRecorded();
    if (type == "None" || !mm)
        return type;
    const bool imp = s == Units::System::Imperial;
    const QString amount = (type == "Snow")
        ? number(Units::snowMmToDisplay(*mm, s), imp ? 2 : 1) + " " + Units::snowUnit(s)
        : number(Units::mmToDisplay(*mm, s), imp ? 2 : 1) + " " + Units::rainUnit(s);
    return type.isEmpty() ? amount : type + ", " + amount;
}

QString describeDay(const QDate &date, const std::optional<WeatherDay> &weather,
                    bool hasNote, bool hasEvent, Units::System s)
{
    QString out = date.toString("dddd, MMMM d, yyyy") + ".";
    if (weather) {
        QStringList parts;
        if (!weather->cloud.isEmpty())
            parts << weather->cloud;
        if (weather->precipType == "None")
            parts << "no precipitation";
        else if (!weather->precipType.isEmpty())
            parts << precipitation(weather->precipType, weather->precipMm, s).toLower();
        if (weather->highC)
            parts << "high " + temperature(weather->highC, s);
        if (weather->lowC)
            parts << "low " + temperature(weather->lowC, s);
        out += " " + (parts.isEmpty() ? QString("Weather recorded") : parts.join(", ")) + ".";
    }
    if (hasNote)
        out += " Has a note.";
    if (hasEvent)
        out += " Has an event.";
    if (!weather && !hasNote && !hasEvent)
        out += " Nothing recorded.";
    return out;
}

} // namespace Format
