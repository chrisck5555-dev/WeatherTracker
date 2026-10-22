#include <QtTest>

#include "StatsRows.h"

static QString valueOf(const QList<StatsRows::Row> &rows, const QString &label)
{
    for (const auto &r : rows)
        if (r.label == label)
            return r.value;
    return QStringLiteral("<missing>");
}

static WeatherDay sample(double hi, double lo)
{
    WeatherDay d;
    d.highC = hi;
    d.lowC = lo;
    return d;
}

class TestStatsRows : public QObject
{
    Q_OBJECT

private slots:
    void emptyPeriod()
    {
        QVERIFY(StatsRows::build(Stats::summarize({}), 30, false, Units::System::Metric).isEmpty());
    }

    void multiDayLabelsAndCounts()
    {
        const auto s = Stats::summarize({sample(10, 0), sample(20, 4)});
        const auto rows = StatsRows::build(s, 30, false, Units::System::Metric);
        QCOMPARE(valueOf(rows, "Days recorded"), QString("2 of 30 days"));
        QCOMPARE(valueOf(rows, "Average high"), QString("15 \u00B0C  (based on 2 days)"));
        QCOMPARE(valueOf(rows, "Average wind speed"), QString("Not recorded"));
        QCOMPARE(valueOf(rows, "Average wind direction"), QString("Not recorded"));
    }

    void singleDayLabels()
    {
        const auto s = Stats::summarize({sample(10, 0)});
        const auto rows = StatsRows::build(s, 1, true, Units::System::Metric);
        QCOMPARE(valueOf(rows, "High"), QString("10 \u00B0C"));         // no "based on"
        QCOMPARE(valueOf(rows, "Wind direction"), QString("Not recorded"));
        QCOMPARE(valueOf(rows, "Cloud cover"), QString("Not recorded"));
        QCOMPARE(valueOf(rows, "Days recorded"), QString("<missing>")); // dropped for one day
        QCOMPARE(valueOf(rows, "Average high"), QString("<missing>"));
    }

    void imperialConverts()
    {
        const auto s = Stats::summarize({sample(0, -10)});
        const auto rows = StatsRows::build(s, 1, true, Units::System::Imperial);
        QCOMPARE(valueOf(rows, "High"), QString("32 \u00B0F"));
        QCOMPARE(valueOf(rows, "Low"), QString("14 \u00B0F"));
    }

    void precipitationBreakdown()
    {
        WeatherDay rain; rain.precipType = "Rain"; rain.precipMm = 6.0;
        WeatherDay snow; snow.precipType = "Snow"; snow.precipMm = 100.0;
        const auto rows = StatsRows::build(Stats::summarize({rain, snow}), 30, false,
                                           Units::System::Metric);
        QCOMPARE(valueOf(rows, "Total precipitation"), QString("106 mm"));
        QCOMPARE(valueOf(rows, "Rain"), QString("1 day, 6 mm"));
        QCOMPARE(valueOf(rows, "Snow"), QString("1 day, 10 cm"));       // depth, not mm
    }
};

QTEST_APPLESS_MAIN(TestStatsRows)
#include "tst_statsrows.moc"