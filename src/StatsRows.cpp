#include "StatsRows.h"

#include <functional>

#include "Format.h"

namespace StatsRows {

namespace {

QString dayCount(int n) { return QString("%1 day%2").arg(n).arg(n == 1 ? "" : "s"); }

} // namespace

QList<Row> build(const Stats::Summary &s, int daysInPeriod, bool single, Units::System u)
{
    QList<Row> rows;
    if (s.daysRecorded == 0)
        return rows;

    const bool imp = u == Units::System::Imperial;
    using Opt = std::optional<double>;

    // "21.3 °C  (based on 5 days)"; the count is left out for a single day.
    auto avg = [&](const Stats::Average &a, const std::function<QString(const Opt &)> &fmt) {
        if (!a.value)
            return Format::notRecorded();
        QString text = fmt(a.value);
        if (!single)
            text += QString("  (based on %1)").arg(dayCount(a.count));
        return text;
    };
    auto label = [&](const QString &multi, const QString &one) { return single ? one : multi; };

    auto temp = [u](const Opt &v) { return Format::temperature(v, u); };
    auto wind = [u](const Opt &v) { return Format::windSpeed(v, u); };
    auto press = [u](const Opt &v) { return Format::pressure(v, u); };
    auto humid = [](const Opt &v) { return Format::humidity(v); };

    auto mmText = [&](double mm) {
        return Format::number(Units::mmToDisplay(mm, u), imp ? 2 : 1) + " " + Units::rainUnit(u);
    };

    if (!single)
        rows.append({"Days recorded",
                     QString("%1 of %2 days").arg(s.daysRecorded).arg(daysInPeriod)});

    rows.append({label("Average high", "High"), avg(s.high, temp)});
    rows.append({label("Average low", "Low"), avg(s.low, temp)});
    rows.append({label("Average temperature", "Mean temperature"), avg(s.mean, temp)});
    rows.append({label("Average wind speed", "Wind speed"), avg(s.wind, wind)});
    rows.append({label("Average wind direction", "Wind direction"),
                 s.windDirection.isEmpty() ? Format::notRecorded() : s.windDirection});
    rows.append({label("Average humidity", "Humidity"), avg(s.humidity, humid)});
    rows.append({label("Average pressure", "Pressure"), avg(s.pressure, press)});
    rows.append({label("Most common cloud", "Cloud cover"),
                 s.cloudModes.isEmpty() ? Format::notRecorded() : s.cloudModes.join(" / ")});

    if (single) {
        const QString type = s.precipByType.isEmpty() ? QString() : s.precipByType.first().type;
        rows.append({"Precipitation", Format::precipitation(type, s.precipTotalMm, u)});
        return rows;
    }

    rows.append({"Total precipitation",
                 s.precipTotalMm ? mmText(*s.precipTotalMm) : Format::notRecorded()});
    rows.append({"Precipitation per recorded day",
                 s.precipPerDayMm ? mmText(*s.precipPerDayMm) : Format::notRecorded()});

    if (!s.precipByType.isEmpty()) {
        Row h;
        h.label = "Precipitation by type";
        h.header = true;
        rows.append(h);
        for (const Stats::PrecipRow &r : s.precipByType) {
            QString text = dayCount(r.days);
            if (r.type != "None") {
                const double shown = r.type == "Snow" ? Units::snowMmToDisplay(r.totalMm, u)
                                                      : Units::mmToDisplay(r.totalMm, u);
                const QString unit = r.type == "Snow" ? Units::snowUnit(u) : Units::rainUnit(u);
                text += ", " + Format::number(shown, imp ? 2 : 1) + " " + unit;
            }
            rows.append({r.type, text});
        }
    }
    return rows;
}

} // namespace StatsRows