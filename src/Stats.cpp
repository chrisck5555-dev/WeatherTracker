#include "Stats.h"

#include <QMap>
#include <algorithm>
#include <cmath>

#include "WeatherInput.h"

namespace Stats {

namespace {

constexpr double kPi = 3.14159265358979323846;

const QStringList &compassNames()
{
    static const QStringList l = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return l;
}

// Running average that skips missing values.
struct Accumulator {
    double sum = 0;
    int count = 0;
    void add(const std::optional<double> &v)
    {
        if (!v) return;
        sum += *v;
        ++count;
    }
    Average result() const
    {
        Average a;
        a.count = count;
        if (count > 0)
            a.value = sum / count;
        return a;
    }
};

} // namespace

QString compassName(int index) { return compassNames().value(((index % 8) + 8) % 8); }

std::optional<double> compassDegrees(const QString &name)
{
    const int i = compassNames().indexOf(name);
    if (i < 0) return std::nullopt;
    return i * 45.0;
}

int compassIndex(double degrees)
{
    double d = std::fmod(degrees, 360.0);
    if (d < 0) d += 360.0;
    // 22.5 is exactly halfway between N and NE. floor(x + 0.5) rounds halves UP,
    // which is the clockwise direction, so exact ties go clockwise.
    const int i = static_cast<int>(std::floor(d / 45.0 + 0.5));
    return i % 8;
}

QString averageWindDirection(const QStringList &directions, double threshold)
{
    double sumSin = 0, sumCos = 0;
    int n = 0;
    for (const QString &name : directions) {
        const auto deg = compassDegrees(name);
        if (!deg) continue;                               // unknown text is ignored
        const double rad = *deg * kPi / 180.0;
        sumSin += std::sin(rad);
        sumCos += std::cos(rad);
        ++n;
    }
    if (n == 0)
        return QString();
    const double length = std::hypot(sumSin, sumCos) / n; // 1 = all agree, 0 = cancel out
    if (length < threshold)
        return QStringLiteral("Variable");
    const double deg = std::atan2(sumSin, sumCos) * 180.0 / kPi;
    return compassName(compassIndex(deg));
}

QStringList modes(const QStringList &values)
{
    QStringList order;                                    // first-seen order, no duplicates
    QMap<QString, int> counts;
    for (const QString &v : values) {
        if (v.isEmpty()) continue;
        if (!counts.contains(v)) order << v;
        counts[v] += 1;
    }
    int best = 0;
    for (const QString &v : order)
        best = std::max(best, counts[v]);
    QStringList result;
    for (const QString &v : order)
        if (counts[v] == best)
            result << v;
    return result;
}

Summary summarize(const QList<WeatherDay> &days)
{
    Summary s;
    Accumulator high, low, mean, wind, humidity, pressure;
    QStringList directions, clouds;
    double precipSum = 0;
    QMap<QString, PrecipRow> byType;

    for (const WeatherDay &d : days) {
        if (d.isEmpty())
            continue;                                     // no row = not a recorded day
        ++s.daysRecorded;

        high.add(d.highC);
        low.add(d.lowC);
        if (d.highC && d.lowC)
            mean.add((*d.highC + *d.lowC) / 2.0);         // only days with BOTH
        wind.add(d.windKmh);
        humidity.add(d.humidityPct);
        pressure.add(d.pressureHpa);

        if (!d.windDir.isEmpty()) directions << d.windDir;
        if (!d.cloud.isEmpty()) clouds << d.cloud;

        // "None" is stored as 0 mm, so any day with a figure counts, including zero.
        if (d.precipMm) {
            precipSum += *d.precipMm;
            ++s.precipDays;
        }
        if (!d.precipType.isEmpty()) {
            PrecipRow &row = byType[d.precipType];
            row.type = d.precipType;
            row.days += 1;
            row.totalMm += d.precipMm.value_or(0.0);
        }
    }

    s.high = high.result();
    s.low = low.result();
    s.mean = mean.result();
    s.wind = wind.result();
    s.humidity = humidity.result();
    s.pressure = pressure.result();

    if (s.precipDays > 0) {
        s.precipTotalMm = precipSum;
        s.precipPerDayMm = precipSum / s.precipDays;
    }

    s.windDirection = averageWindDirection(directions);
    s.cloudModes = modes(clouds);

    // Display order follows the editor's dropdown, not alphabetical.
    for (const QString &type : WeatherInput::precipTypes())
        if (byType.contains(type))
            s.precipByType << byType[type];
    return s;
}

} // namespace Stats