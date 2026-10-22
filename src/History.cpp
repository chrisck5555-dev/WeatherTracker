#include "History.h"

#include <cmath>

#include "Format.h"
#include "PeriodMath.h"

namespace History {

namespace {

struct Record {                       // one earlier year that has this exact date
    PeriodMath::EarlierDate when;
    WeatherDay day;
};

bool isSnow(const WeatherDay &d) { return d.precipType == "Snow"; }
bool isRain(const WeatherDay &d)
{
    return !d.precipType.isEmpty() && d.precipType != "Snow" && d.precipType != "None";
}

QString yearLabel(const PeriodMath::EarlierDate &e)
{
    return e.usedMarch1 ? QString("%1 (March 1)").arg(e.date.year())
                        : QString::number(e.date.year());
}

// "High is 3 °C warmer than usual (usual 8 °C)". The gap is a DIFFERENCE, so it
// is converted without the +32 that an absolute temperature needs.
QString versus(const QString &name, double today, double usual, Units::System u)
{
    const double delta = Units::deltaCToDisplay(today - usual, u);
    const QString usualText = Format::temperature(usual, u);
    if (std::abs(delta) < 0.5)
        return QString("%1 is about usual (%2)").arg(name, usualText);
    return QString("%1 is %2 %3 than usual (usual %4)")
        .arg(name, Format::number(std::abs(delta), 1) + " " + Units::tempUnit(u),
             delta > 0 ? "warmer" : "cooler", usualText);
}

// Running totals for a stretch of days. Blanks are skipped, never counted as zero.
struct Tally {
    int recorded = 0;
    double highSum = 0, lowSum = 0, rainMm = 0, snowMm = 0;
    int highN = 0, lowN = 0, precipN = 0, wet = 0;

    void add(const WeatherDay &d)
    {
        if (d.isEmpty()) return;
        ++recorded;
        if (d.highC) { highSum += *d.highC; ++highN; }
        if (d.lowC)  { lowSum += *d.lowC; ++lowN; }
        if (d.precipMm) {
            ++precipN;
            if (*d.precipMm > 0) ++wet;
            if (isRain(d)) rainMm += *d.precipMm;
            else if (isSnow(d)) snowMm += *d.precipMm;
        }
    }
    void add(const Tally &o)
    {
        recorded += o.recorded;
        highSum += o.highSum;  highN += o.highN;
        lowSum += o.lowSum;    lowN += o.lowN;
        rainMm += o.rainMm;    snowMm += o.snowMm;
        precipN += o.precipN;  wet += o.wet;
    }
    std::optional<double> highAvg() const
    {
        if (highN == 0) return std::nullopt;
        return highSum / highN;
    }
    std::optional<double> lowAvg() const
    {
        if (lowN == 0) return std::nullopt;
        return lowSum / lowN;
    }
};

struct EarlierPeriod {
    QDate start, end;
    Tally tally;
};

QString periodLabel(const QDate &start, Period p)
{
    if (p == Period::Season && start.month() == 12)          // Winter spans New Year
        return QString("%1\u2013%2").arg(start.year())
                   .arg((start.year() + 1) % 100, 2, 10, QChar('0'));
    return QString::number(start.year());
}

// A difference between two averages, so no +32 (same rule as the day panel).
QString versusEarlier(const QString &name, double now, double before, Units::System u)
{
    const double delta = Units::deltaCToDisplay(now - before, u);
    const QString beforeText = Format::temperature(before, u);
    if (std::abs(delta) < 0.5)
        return QString("%1 is about the same as in earlier years (%2)").arg(name, beforeText);
    return QString("%1 is %2 %3 than in earlier years (they averaged %4)")
        .arg(name, Format::number(std::abs(delta), 1) + " " + Units::tempUnit(u),
             delta > 0 ? "warmer" : "cooler", beforeText);
}

QString precipAmount(double mm, bool snow, Units::System u)
{
    const bool imp = u == Units::System::Imperial;
    const double shown = snow ? Units::snowMmToDisplay(mm, u) : Units::mmToDisplay(mm, u);
    return Format::number(shown, imp ? 2 : 1) + " "
         + (snow ? Units::snowUnit(u) : Units::rainUnit(u));
}
} // namespace

DayComparison compareDay(const QDate &date, const std::optional<WeatherDay> &thisDay,
                         const QMap<QDate, WeatherDay> &earlier, Units::System u)
{
    DayComparison out;
    if (earlier.isEmpty())
        return out;                                   // hidden completely
    out.visible = true;
    const bool imp = u == Units::System::Imperial;

    QList<Record> records;                            // newest year first
    double highSum = 0, lowSum = 0;
    int highN = 0, lowN = 0;

    const int firstYear = earlier.firstKey().year();
    for (int y = date.year() - 1; y >= firstYear; --y) {
        const auto when = PeriodMath::sameDateInYear(date, y);   // Feb 29 rule
        const auto exact = earlier.constFind(when.date);
        if (exact != earlier.constEnd() && !exact->isEmpty())
            records.append(Record{when, *exact});

        for (int off = -kWindowDays; off <= kWindowDays; ++off) {
            const auto w = earlier.constFind(when.date.addDays(off));
            if (w == earlier.constEnd())
                continue;
            if (w->highC) { highSum += *w->highC; ++highN; }
            if (w->lowC)  { lowSum += *w->lowC; ++lowN; }
        }
    }

    // 1. This day against the usual for the time of year
    if (thisDay) {
        QStringList parts;
        if (thisDay->highC && highN >= kMinWindowPoints)
            parts << versus("High", *thisDay->highC, highSum / highN, u);
        if (thisDay->lowC && lowN >= kMinWindowPoints)
            parts << versus("Low", *thisDay->lowC, lowSum / lowN, u);
        if (!parts.isEmpty())
            out.highlights << parts.join("; ") + ".";
    }

    // 2. How often precipitation was recorded on this date
    int withFigure = 0, wet = 0;
    for (const Record &r : records) {
        if (!r.day.precipMm) continue;
        ++withFigure;
        if (*r.day.precipMm > 0) ++wet;
    }
    if (withFigure >= kMinYears)
        out.highlights << QString("Precipitation was recorded on this date in %1 of %2 earlier years.")
                              .arg(wet).arg(withFigure);

    // 3. Warmest and coldest earlier year (records are newest first, and strict
    //    comparisons keep the first one found, so ties go to the newest year)
    const Record *warm = nullptr, *cold = nullptr;
    int withHigh = 0;
    for (const Record &r : records) {
        if (!r.day.highC) continue;
        ++withHigh;
        if (!warm || *r.day.highC > *warm->day.highC) warm = &r;
        if (!cold || *r.day.highC < *cold->day.highC) cold = &r;
    }
    if (withHigh >= kMinYears)
        out.highlights << QString("Warmest: %1 (high %2). Coldest: %3 (high %4).")
                              .arg(warm->when.date.year())
                              .arg(Format::temperature(warm->day.highC, u))
                              .arg(cold->when.date.year())
                              .arg(Format::temperature(cold->day.highC, u));

    // 4. Most rain and deepest snow: two separate stats, never combined
    const Record *rain = nullptr, *snow = nullptr;
    for (const Record &r : records) {
        if (!r.day.precipMm || *r.day.precipMm <= 0) continue;
        if (isRain(r.day) && (!rain || *r.day.precipMm > *rain->day.precipMm)) rain = &r;
        if (isSnow(r.day) && (!snow || *r.day.precipMm > *snow->day.precipMm)) snow = &r;
    }
    QStringList extremes;
    if (rain)
        extremes << QString("Most rain: %1 %2 (%3).")
                        .arg(Format::number(Units::mmToDisplay(*rain->day.precipMm, u), imp ? 2 : 1))
                        .arg(Units::rainUnit(u)).arg(rain->when.date.year());
    if (snow)
        extremes << QString("Deepest snow: %1 %2 (%3).")
                        .arg(Format::number(Units::snowMmToDisplay(*snow->day.precipMm, u), imp ? 2 : 1))
                        .arg(Units::snowUnit(u)).arg(snow->when.date.year());
    if (!extremes.isEmpty())
        out.highlights << extremes.join(" ");

    // One block per earlier year with this exact date. Notes and events are left out.
    for (const Record &r : records) {
        YearBlock b;
        b.title = yearLabel(r.when);
        b.lines << "High " + Format::temperature(r.day.highC, u)
                       + "   Low " + Format::temperature(r.day.lowC, u);
        b.lines << "Precipitation: " + Format::precipitation(r.day.precipType, r.day.precipMm, u);
        out.blocks << b;
    }
    if (out.blocks.isEmpty())
        out.emptyMessage = "Nothing was recorded on this date in earlier years.";
    return out;
}

PeriodComparison comparePeriod(const QDate &date, Period p,
                               const QMap<QDate, WeatherDay> &current,
                               const QMap<QDate, WeatherDay> &earlier, Units::System u)
{
    PeriodComparison out;
    if (earlier.isEmpty() || p == Period::Day || p == Period::Week)
        return out;                                   // hidden completely
    out.visible = true;

    Tally now;
    for (const WeatherDay &d : current)
        now.add(d);

    // Walk back one year at a time, keeping periods with enough recorded days.
    const QDate curStart = PeriodMath::startOf(date, p);
    const QDate firstDate = earlier.firstKey();
    QList<EarlierPeriod> periods;                     // newest first
    Tally pool;
    for (int n = 1; n < 400; ++n) {
        const QDate s = curStart.addYears(-n);        // always a 1st of a month: safe
        const QDate e = PeriodMath::endOf(s, p);
        if (e < firstDate)
            break;                                    // nothing older can exist
        EarlierPeriod ep;
        ep.start = s;
        ep.end = e;
        for (auto it = earlier.lowerBound(s); it != earlier.constEnd() && it.key() <= e; ++it)
            ep.tally.add(it.value());
        if (ep.tally.recorded >= kMinPeriodDays) {
            pool.add(ep.tally);
            periods.append(ep);
        }
    }
    if (periods.isEmpty()) {
        out.emptyMessage = QString("No earlier year has %1 or more recorded days for this "
                                   "period yet, so there is nothing to compare.")
                               .arg(kMinPeriodDays);
        return out;
    }

    // 1. Average high and low against earlier years
    QStringList temps;
    if (now.highN >= kMinSidePoints && pool.highN >= kMinSidePoints)
        temps << versusEarlier("Average high", now.highSum / now.highN,
                               pool.highSum / pool.highN, u);
    if (now.lowN >= kMinSidePoints && pool.lowN >= kMinSidePoints)
        temps << versusEarlier("Average low", now.lowSum / now.lowN,
                               pool.lowSum / pool.lowN, u);
    if (!temps.isEmpty())
        out.highlights << temps.join("; ") + ".";

    const bool precipComparable = now.precipN >= kMinSidePoints && pool.precipN >= kMinSidePoints;

    // 2. Share of days with precipitation
    if (precipComparable)
        out.highlights << QString("Precipitation was recorded on %1% of days (earlier years: %2%).")
                              .arg(qRound(100.0 * now.wet / now.precipN))
                              .arg(qRound(100.0 * pool.wet / pool.precipN));

    // 3. Rain and snow per recorded day. Two separate stats, never combined.
    if (precipComparable) {
        QStringList per;
        const double rainNow = now.rainMm / now.precipN, rainPool = pool.rainMm / pool.precipN;
        const double snowNow = now.snowMm / now.precipN, snowPool = pool.snowMm / pool.precipN;
        if (rainNow > 0 || rainPool > 0)
            per << QString("Rain per recorded day: %1 (earlier years: %2).")
                       .arg(precipAmount(rainNow, false, u), precipAmount(rainPool, false, u));
        if (snowNow > 0 || snowPool > 0)
            per << QString("Snow per recorded day: %1 (earlier years: %2).")
                       .arg(precipAmount(snowNow, true, u), precipAmount(snowPool, true, u));
        if (!per.isEmpty())
            out.highlights << per.join(" ");
    }

    // 4. Warmest and coldest earlier period, by average high. Periods are newest
    //    first and the comparisons are strict, so ties go to the newest.
    const EarlierPeriod *warm = nullptr, *cold = nullptr;
    int withHigh = 0;
    for (const EarlierPeriod &ep : periods) {
        if (ep.tally.highN < kMinSidePoints) continue;
        ++withHigh;
        const double avg = ep.tally.highSum / ep.tally.highN;
        if (!warm || avg > warm->tally.highSum / warm->tally.highN) warm = &ep;
        if (!cold || avg < cold->tally.highSum / cold->tally.highN) cold = &ep;
    }
    if (withHigh >= kMinYears)
        out.highlights << QString("Warmest: %1 (average high %2). Coldest: %3 (average high %4).")
                              .arg(periodLabel(warm->start, p),
                                   Format::temperature(warm->tally.highAvg(), u),
                                   periodLabel(cold->start, p),
                                   Format::temperature(cold->tally.highAvg(), u));

    // 5. Wettest and snowiest single day inside those earlier periods
    struct Extreme { QDate date; double mm = 0; };
    Extreme wettest, snowiest;                        // mm == 0 means none found
    for (const EarlierPeriod &ep : periods)
        for (auto it = earlier.lowerBound(ep.start);
             it != earlier.constEnd() && it.key() <= ep.end; ++it) {
            const WeatherDay &d = it.value();
            if (!d.precipMm || *d.precipMm <= 0) continue;
            if (isRain(d) && *d.precipMm > wettest.mm)  wettest = {it.key(), *d.precipMm};
            if (isSnow(d) && *d.precipMm > snowiest.mm) snowiest = {it.key(), *d.precipMm};
        }
    QStringList extremes;
    if (wettest.mm > 0)
        extremes << QString("Wettest day: %1 (%2).")
                        .arg(precipAmount(wettest.mm, false, u),
                             wettest.date.toString("MMMM d, yyyy"));
    if (snowiest.mm > 0)
        extremes << QString("Snowiest day: %1 (%2).")
                        .arg(precipAmount(snowiest.mm, true, u),
                             snowiest.date.toString("MMMM d, yyyy"));
    if (!extremes.isEmpty())
        out.highlights << extremes.join(" ");

    // One block per counting earlier period, newest first
    for (const EarlierPeriod &ep : periods) {
        const Tally &t = ep.tally;
        YearBlock b;
        b.title = periodLabel(ep.start, p);
        b.lines << "Average high " + Format::temperature(t.highAvg(), u)
                       + "   Average low " + Format::temperature(t.lowAvg(), u);
        if (t.precipN > 0) {
            b.lines << "Rain per recorded day: " + precipAmount(t.rainMm / t.precipN, false, u)
                           + "   Snow per recorded day: " + precipAmount(t.snowMm / t.precipN, true, u);
            b.lines << QString("Precipitation on %1 of %2 days").arg(t.wet).arg(t.precipN);
        } else {
            b.lines << "Precipitation: " + Format::notRecorded();
        }
        out.blocks << b;
    }
    return out;
}

} // namespace History