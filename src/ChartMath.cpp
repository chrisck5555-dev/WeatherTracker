#include "ChartMath.h"

#include <algorithm>
#include <cmath>

namespace ChartMath {

Axis niceAxis(double lo, double hi, int maxTicks)
{
    if (hi < lo)
        std::swap(lo, hi);
    if (hi - lo < 1e-9) {                      // one value: give it some room
        lo -= 1;
        hi += 1;
    }
    const double rough = (hi - lo) / (maxTicks - 1);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
    const double norm = rough / magnitude;     // between 1 and 10
    const double niceNorm = norm <= 1 ? 1 : norm <= 2 ? 2 : norm <= 5 ? 5 : 10;

    Axis a;
    a.step = niceNorm * magnitude;
    // The 1e-9 absorbs floating-point noise: 0.3 / 0.1 is 2.9999999999999996.
    a.min = std::floor(lo / a.step + 1e-9) * a.step;
    a.max = std::ceil(hi / a.step - 1e-9) * a.step;
    return a;
}

QList<int> thinnedIndices(int count, double availableWidth, double labelWidth, double gap)
{
    QList<int> out;
    if (count <= 0)
        return out;
    const int maxLabels = std::max(1, int(availableWidth / (labelWidth + gap)));
    const int step = (count + maxLabels - 1) / maxLabels;       // round up
    for (int i = 0; i < count; i += step)
        out << i;
    return out;
}

QList<QList<int>> runs(const QList<std::optional<double>> &values)
{
    QList<QList<int>> out;
    QList<int> current;
    for (int i = 0; i < values.size(); ++i) {
        if (values[i]) {
            current << i;
        } else if (!current.isEmpty()) {
            out << current;
            current.clear();
        }
    }
    if (!current.isEmpty())
        out << current;
    return out;
}

TempSeries temperatureSeries(const QDate &from, const QDate &to,
                             const QMap<QDate, WeatherDay> &days, Units::System u)
{
    TempSeries s;
    s.first = from;
    for (QDate d = from; d <= to; d = d.addDays(1)) {
        std::optional<double> hi, lo;
        const auto it = days.constFind(d);
        if (it != days.constEnd()) {
            if (it->highC) hi = Units::cToDisplay(*it->highC, u);
            if (it->lowC)  lo = Units::cToDisplay(*it->lowC, u);
        }
        s.high << hi;                          // an unset value stays unset: that is the gap
        s.low << lo;
    }
    return s;
}

std::optional<Axis> temperatureAxis(const TempSeries &s)
{
    bool any = false;
    double lo = 0, hi = 0;
    for (const auto *list : {&s.high, &s.low})
        for (const auto &v : *list) {
            if (!v) continue;
            if (!any) { lo = hi = *v; any = true; }
            else      { lo = std::min(lo, *v); hi = std::max(hi, *v); }
        }
    if (!any)
        return std::nullopt;
    return niceAxis(lo, hi);
}

namespace {

bool matchesKind(const WeatherDay &d, PrecipKind kind)
{
    if (!d.precipMm || *d.precipMm <= 0)
        return false;
    if (kind == PrecipKind::Snow)
        return d.precipType == "Snow";
    return !d.precipType.isEmpty() && d.precipType != "Snow" && d.precipType != "None";
}

double toDisplay(double mm, PrecipKind kind, Units::System u)
{
    return kind == PrecipKind::Snow ? Units::snowMmToDisplay(mm, u)
                                    : Units::mmToDisplay(mm, u);
}

} // namespace

BarSeries precipSeries(const QDate &from, const QDate &to, Period period,
                       const QMap<QDate, WeatherDay> &days, PrecipKind kind,
                       Units::System u)
{
    BarSeries s;
    s.hasAnyData = !days.isEmpty();

    if (period == Period::Year) {
        double totalMm[12] = {};
        for (auto it = days.constBegin(); it != days.constEnd(); ++it)
            if (matchesKind(it.value(), kind))
                totalMm[it.key().month() - 1] += *it.value().precipMm;
        for (int m = 0; m < 12; ++m) {
            s.values << toDisplay(totalMm[m], kind, u);
            s.labels << QDate(from.year(), m + 1, 1).toString("MMM");
        }
    } else {
        for (QDate d = from; d <= to; d = d.addDays(1)) {
            const auto it = days.constFind(d);
            const double mm = (it != days.constEnd() && matchesKind(*it, kind))
                                  ? *it->precipMm : 0.0;
            s.values << toDisplay(mm, kind, u);
            s.labels << QString::number(d.day());
        }
    }
    for (double v : s.values)
        if (v > 0) { s.hasAnyBar = true; break; }
    return s;
}

Axis barAxis(const BarSeries &s)
{
    double hi = 0;
    for (double v : s.values)
        hi = std::max(hi, v);
    Axis a = niceAxis(0, hi > 0 ? hi : 1);
    a.min = 0;
    return a;
}

LineSeries metricSeries(const QDate &from, const QDate &to,
                        const QMap<QDate, WeatherDay> &days, Metric metric, Units::System u)
{
    LineSeries s;
    s.first = from;
    switch (metric) {
    case Metric::Wind:
        s.names = {"Wind speed", "Gust"};
        s.zeroBased = true;
        break;
    case Metric::Pressure:
        s.names = {"Pressure"};
        break;
    case Metric::Humidity:
        s.names = {"Humidity"};
        s.fixedAxis = Axis{0, 100, 20};
        break;
    }
    s.values.resize(s.names.size());

    for (QDate d = from; d <= to; d = d.addDays(1)) {
        std::optional<double> a, b;
        const auto it = days.constFind(d);
        if (it != days.constEnd()) {
            switch (metric) {
            case Metric::Wind:
                if (it->windKmh) a = Units::kmhToDisplay(*it->windKmh, u);
                if (it->gustKmh) b = Units::kmhToDisplay(*it->gustKmh, u);
                break;
            case Metric::Pressure:
                if (it->pressureHpa) a = Units::hpaToDisplay(*it->pressureHpa, u);
                break;
            case Metric::Humidity:
                a = it->humidityPct;
                break;
            }
        }
        s.values[0] << a;                      // an unset value stays unset: that is the gap
        if (s.values.size() > 1)
            s.values[1] << b;
    }
    return s;
}

std::optional<Axis> lineAxis(const LineSeries &s)
{
    bool any = false;
    double lo = 0, hi = 0;
    for (const auto &list : s.values)
        for (const auto &v : list) {
            if (!v) continue;
            if (!any) { lo = hi = *v; any = true; }
            else      { lo = std::min(lo, *v); hi = std::max(hi, *v); }
        }
    if (!any)
        return std::nullopt;
    if (s.fixedAxis)
        return s.fixedAxis;
    if (s.zeroBased)
        lo = std::min(lo, 0.0);
    Axis a = niceAxis(lo, hi);
    if (s.zeroBased) {
        a.min = 0;
        if (a.max <= 0)
            a.max = a.step;
    }
    return a;
}

} // namespace ChartMath