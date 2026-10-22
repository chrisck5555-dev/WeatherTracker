#include <QtTest>

#include "ChartMath.h"

using ChartMath::Axis;

class TestChartMath : public QObject
{
    Q_OBJECT

private slots:
    void niceAxisExamples()
    {
        Axis a = ChartMath::niceAxis(0, 10);
        QCOMPARE(a.min, 0.0);
        QCOMPARE(a.max, 10.0);
        QCOMPARE(a.step, 2.0);

        a = ChartMath::niceAxis(-4.7, 21.3);
        QCOMPARE(a.min, -10.0);
        QCOMPARE(a.max, 30.0);
        QCOMPARE(a.step, 10.0);

        a = ChartMath::niceAxis(3, 3);                 // a single value
        QVERIFY(a.min < 3 && a.max > 3);

        a = ChartMath::niceAxis(0, 0.3);               // floating-point edge
        QCOMPARE(a.min, 0.0);
        QCOMPARE(a.max, 0.3);
    }

    void niceAxisAlwaysCoversTheData()
    {
        for (double lo = -50; lo <= 50; lo += 3.7)
            for (double span : {0.4, 2.0, 9.0, 26.0, 130.0}) {
                const double hi = lo + span;
                const Axis a = ChartMath::niceAxis(lo, hi);
                QVERIFY(a.min <= lo + 1e-9);
                QVERIFY(a.max >= hi - 1e-9);
                QVERIFY((a.max - a.min) / a.step <= 6 + 1 + 1e-9);   // not too many ticks
            }
    }

    void thinning()
    {
        QCOMPARE(ChartMath::thinnedIndices(5, 1000, 24), (QList<int>{0, 1, 2, 3, 4}));
        const auto t = ChartMath::thinnedIndices(31, 600, 24);        // room for 16 labels
        QCOMPARE(t.first(), 0);
        QCOMPARE(t[1], 2);
        QVERIFY(t.size() <= 16);
        QCOMPARE(ChartMath::thinnedIndices(100, 36, 24), (QList<int>{0}));   // very narrow
        QVERIFY(ChartMath::thinnedIndices(0, 600, 24).isEmpty());
    }

    void runsSplitAtGaps()
    {
        const QList<std::optional<double>> v{1.0, 2.0, std::nullopt, 4.0, std::nullopt,
                                             std::nullopt, 7.0, 8.0};
        const auto r = ChartMath::runs(v);
        QCOMPARE(int(r.size()), 3);
        QCOMPARE(r[0], (QList<int>{0, 1}));
        QCOMPARE(r[1], (QList<int>{3}));
        QCOMPARE(r[2], (QList<int>{6, 7}));
        QVERIFY(ChartMath::runs({std::nullopt, std::nullopt}).isEmpty());
    }

    void seriesKeepsGapsAndConverts()
    {
        QMap<QDate, WeatherDay> days;
        WeatherDay a; a.highC = 0.0; a.lowC = -5.0;
        WeatherDay b; b.highC = 10.0;
        days.insert(QDate(2026, 10, 1), a);
        days.insert(QDate(2026, 10, 3), b);

        const auto metric = ChartMath::temperatureSeries(QDate(2026, 10, 1), QDate(2026, 10, 3),
                                                         days, Units::System::Metric);
        QCOMPARE(metric.days(), 3);
        QVERIFY(!metric.high[1].has_value());              // Oct 2: no row = gap
        QVERIFY(!metric.low[2].has_value());               // Oct 3: no low

        const auto imp = ChartMath::temperatureSeries(QDate(2026, 10, 1), QDate(2026, 10, 3),
                                                      days, Units::System::Imperial);
        QCOMPARE(*imp.high[0], 32.0);
        QCOMPARE(*imp.low[0], 23.0);
    }

    void axisNeedsData()
    {
        const auto empty = ChartMath::temperatureSeries(QDate(2026, 10, 1), QDate(2026, 10, 5),
                                                        {}, Units::System::Metric);
        QVERIFY(!ChartMath::temperatureAxis(empty).has_value());
    }

        void rainAndSnowAreSeparate()
    {
        QMap<QDate, WeatherDay> days;
        WeatherDay rain; rain.precipType = "Rain"; rain.precipMm = 6.0;
        WeatherDay snow; snow.precipType = "Snow"; snow.precipMm = 100.0;     // 10 cm
        WeatherDay none; none.precipType = "None"; none.precipMm = 0.0;
        days.insert(QDate(2026, 10, 1), rain);
        days.insert(QDate(2026, 10, 2), snow);
        days.insert(QDate(2026, 10, 3), none);

        const auto r = ChartMath::precipSeries(QDate(2026, 10, 1), QDate(2026, 10, 31),
            Period::Month, days, ChartMath::PrecipKind::Rain, Units::System::Metric);
        QCOMPARE(r.values.size(), 31);
        QCOMPARE(r.values[0], 6.0);
        QCOMPARE(r.values[1], 0.0);                 // snow day is not rain
        QVERIFY(r.hasAnyBar);

        const auto s = ChartMath::precipSeries(QDate(2026, 10, 1), QDate(2026, 10, 31),
            Period::Month, days, ChartMath::PrecipKind::Snow, Units::System::Metric);
        QCOMPARE(s.values[1], 10.0);                // depth in cm, not mm
        QCOMPARE(s.values[0], 0.0);
    }

    void yearUsesMonthlyTotals()
    {
        QMap<QDate, WeatherDay> days;
        for (int d : {3, 9}) {
            WeatherDay w; w.precipType = "Rain"; w.precipMm = 5.0;
            days.insert(QDate(2026, 2, d), w);
        }
        const auto r = ChartMath::precipSeries(QDate(2026, 1, 1), QDate(2026, 12, 31),
            Period::Year, days, ChartMath::PrecipKind::Rain, Units::System::Metric);
        QCOMPARE(r.values.size(), 12);
        QCOMPARE(r.values[1], 10.0);                // February total
        QCOMPARE(r.labels[1], QString("Feb"));
    }

    void noSnowMeansNoBars()
    {
        QMap<QDate, WeatherDay> days;
        WeatherDay w; w.precipType = "Rain"; w.precipMm = 2.0;
        days.insert(QDate(2026, 10, 1), w);
        const auto s = ChartMath::precipSeries(QDate(2026, 10, 1), QDate(2026, 10, 31),
            Period::Month, days, ChartMath::PrecipKind::Snow, Units::System::Metric);
        QVERIFY(!s.hasAnyBar);
        QVERIFY(s.hasAnyData);
    }

    void barAxisStartsAtZero()
    {
        ChartMath::BarSeries s;
        s.values = {0.0, 3.2, 7.9};
        const auto a = ChartMath::barAxis(s);
        QCOMPARE(a.min, 0.0);
        QVERIFY(a.max >= 7.9);
        ChartMath::BarSeries empty;
        empty.values = {0.0, 0.0};
        QVERIFY(ChartMath::barAxis(empty).max >= 1.0);   // no data: still a usable axis
    }
};

QTEST_APPLESS_MAIN(TestChartMath)
#include "tst_chartmath.moc"