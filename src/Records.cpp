#include "Records.h"
#include <functional>

#include <optional>

#include "Format.h"

namespace Records {

namespace {

struct Best {
    std::optional<double> value;
    QDate date;
};

// Keeps the biggest (or smallest) value. Ties keep the earliest day, because the
// days are visited in date order and only a strictly better value replaces it.
void consider(Best &best, const std::optional<double> &v, const QDate &d, bool higher)
{
    if (!v)
        return;
    if (!best.value || (higher ? *v > *best.value : *v < *best.value)) {
        best.value = *v;
        best.date = d;
    }
}

struct Streak {
    int length = 0;
    QDate start;
    QDate end;
};

QString shortDate(const QDate &d) { return d.toString("MMM d, yyyy"); }

QString streakText(const Streak &s, const QString &word)
{
    if (s.length == 0)
        return Format::notRecorded();
    const QString days = s.length == 1 ? QString("1 %1 day").arg(word)
                                       : QString("%1 %2 days").arg(s.length).arg(word);
    if (s.length == 1)
        return days + " (" + shortDate(s.start) + ")";
    return days + " (" + shortDate(s.start) + " to " + shortDate(s.end) + ")";
}

Item header(const QString &text)
{
    Item i;
    i.label = text;
    i.header = true;
    return i;
}

Item record(const QString &label, const Best &best, const std::function<QString(double)> &fmt)
{
    Item i;
    i.label = label;
    if (!best.value) {
        i.value = Format::notRecorded();
        return i;
    }
    i.value = fmt(*best.value) + " on " + shortDate(best.date);
    i.date = best.date;
    return i;
}

} // namespace

QList<Item> build(const QMap<QDate, WeatherDay> &days, Units::System u, const QDate &today)
{
    QList<Item> rows;
    bool any = false;
    for (const WeatherDay &d : days)
        if (!d.isEmpty()) { any = true; break; }
    if (!any)
        return rows;

    const bool imp = u == Units::System::Imperial;
    const int decimals = imp ? 2 : 1;

    Best hottest, coldest, wettest, snowiest, windiest, gustiest;
    int above = 0, aboveOf = 0, below = 0, belowOf = 0;
    constexpr double kHotC = 25.0;
    constexpr double kColdC = -35.0;

    for (auto it = days.constBegin(); it != days.constEnd(); ++it) {
        const WeatherDay &d = it.value();
        const QDate date = it.key();
        consider(hottest, d.highC, date, true);
        consider(coldest, d.lowC, date, false);
        consider(windiest, d.windKmh, date, true);
        consider(gustiest, d.gustKmh, date, true);
        if (d.precipMm && *d.precipMm > 0) {
            if (d.precipType == "Snow")
                consider(snowiest, d.precipMm, date, true);
            else if (!d.precipType.isEmpty() && d.precipType != "None")
                consider(wettest, d.precipMm, date, true);
        }
        if (d.highC) { ++aboveOf; if (*d.highC > kHotC) ++above; }
        if (d.lowC)  { ++belowOf; if (*d.lowC < kColdC) ++below; }
    }

    rows << header("Records");
    rows << record("Hottest day", hottest,
                   [&](double v) { return Format::temperature(v, u); });
    rows << record("Coldest day", coldest,
                   [&](double v) { return Format::temperature(v, u); });
    rows << record("Wettest day (rain)", wettest, [&](double v) {
        return Format::number(Units::mmToDisplay(v, u), decimals) + " " + Units::rainUnit(u);
    });
    rows << record("Snowiest day", snowiest, [&](double v) {
        return Format::number(Units::snowMmToDisplay(v, u), decimals) + " " + Units::snowUnit(u);
    });
    rows << record("Windiest day (average wind)", windiest,
                   [&](double v) { return Format::windSpeed(v, u); });
    rows << record("Highest gust", gustiest,
                   [&](double v) { return Format::windSpeed(v, u); });

    // --- Streaks: walk every calendar day. A day with no precipitation figure
    // is unknown, so it ends a streak (it is neither dry nor wet).
    QDate first = days.firstKey();
    QDate last = days.lastKey();
    if (last > today)
        last = today;
    Streak curDry, curWet, bestDry, bestWet;
    for (QDate d = first; d <= last; d = d.addDays(1)) {
        const auto it = days.constFind(d);
        const bool known = it != days.constEnd() && it->precipMm.has_value();
        const bool dry = known && *it->precipMm <= 0;
        const bool wet = known && *it->precipMm > 0;
        auto extend = [&](Streak &cur, Streak &best, bool on) {
            if (!on) { cur = Streak(); return; }
            if (cur.length == 0)
                cur.start = d;
            cur.end = d;
            ++cur.length;
            if (cur.length > best.length)
                best = cur;
        };
        extend(curDry, bestDry, dry);
        extend(curWet, bestWet, wet);
    }

    rows << header("Streaks");
    Item dryItem;
    dryItem.label = "Longest dry streak";
    dryItem.value = streakText(bestDry, "dry");
    if (bestDry.length > 0) dryItem.date = bestDry.start;
    rows << dryItem;
    Item wetItem;
    wetItem.label = "Longest wet streak (any precipitation)";
    wetItem.value = streakText(bestWet, "wet");
    if (bestWet.length > 0) wetItem.date = bestWet.start;
    rows << wetItem;

    if (last >= today.addDays(-1)) {              // the period reaches up to now
        Item cur;
        cur.label = "Current streak";
        if (curDry.length > 0)
            cur.value = streakText(curDry, "dry");
        else if (curWet.length > 0)
            cur.value = streakText(curWet, "wet");
        else
            cur.value = "None";
        rows << cur;
    }

    // --- Counts, in the units she sees
    const QString hotLimit = Format::number(Units::cToDisplay(kHotC, u), 0) + " " + Units::tempUnit(u);
    const QString coldLimit = Format::number(Units::cToDisplay(kColdC, u), 0) + " " + Units::tempUnit(u);
    auto countText = [](int n, int of, const QString &what) {
        if (of == 0)
            return Format::notRecorded();
        return QString("%1 of %2 days with a recorded %3").arg(n).arg(of).arg(what);
    };
    rows << header("Hot and cold days");
    rows << Item{"Days above " + hotLimit, countText(above, aboveOf, "high"), QDate(), false};
    rows << Item{"Days below " + coldLimit, countText(below, belowOf, "low"), QDate(), false};
    return rows;
}

} // namespace Records
