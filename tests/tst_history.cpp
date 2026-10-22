#include <QtTest>

#include "History.h"

using Units::System;

static WeatherDay wd(std::optional<double> hi, std::optional<double> lo = std::nullopt)
{
    WeatherDay d;
    d.highC = hi;
    d.lowC = lo;
    return d;
}

static bool anyContains(const History::DayComparison &c, const QString &text)
{
    return c.highlights.join("\n").contains(text);
}

static QMap<QDate, WeatherDay> daysOf(int y, int m, int from, int to, double hi, double lo)
{
    QMap<QDate, WeatherDay> map;
    for (int d = from; d <= to; ++d)
        map.insert(QDate(y, m, d), wd(hi, lo));
    return map;
}

static bool periodHas(const History::PeriodComparison &c, const QString &text)
{
    return c.highlights.join("\n").contains(text);
}

class TestHistory : public QObject
{
    Q_OBJECT

private slots:
    void hiddenWithoutEarlierData()
    {
        const auto c = History::compareDay(QDate(2026, 10, 5), std::nullopt, {}, System::Metric);
        QVERIFY(!c.visible);
    }

    void noMatchSaysSo()
    {
        QMap<QDate, WeatherDay> e;
        e.insert(QDate(2025, 3, 10), wd(5.0));          // a different date
        const auto c = History::compareDay(QDate(2026, 10, 5), std::nullopt, e, System::Metric);
        QVERIFY(c.visible);
        QVERIFY(c.blocks.isEmpty());
        QVERIFY(!c.emptyMessage.isEmpty());
    }

    void blocksNewestFirstSkippingGaps()
    {
        QMap<QDate, WeatherDay> e;
        WeatherDay a = wd(12.0, 3.0);
        a.precipType = "Rain";
        a.precipMm = 3.2;
        e.insert(QDate(2024, 10, 5), a);
        e.insert(QDate(2022, 10, 5), wd(8.0));          // 2023 has nothing
        const auto c = History::compareDay(QDate(2026, 10, 5), std::nullopt, e, System::Metric);
        QCOMPARE(int(c.blocks.size()), 2);
        QCOMPARE(c.blocks[0].title, QString("2024"));
        QCOMPARE(c.blocks[1].title, QString("2022"));
        QCOMPARE(c.blocks[0].lines[0], QString("High 12 \u00B0C   Low 3 \u00B0C"));
        QCOMPARE(c.blocks[0].lines[1], QString("Precipitation: Rain, 3.2 mm"));
    }

    void feb29UsesMarch1InNormalYears()
    {
        QMap<QDate, WeatherDay> e;
        e.insert(QDate(2025, 3, 1), wd(5.0));
        e.insert(QDate(2024, 2, 29), wd(7.0));
        const auto c = History::compareDay(QDate(2028, 2, 29), std::nullopt, e, System::Metric);
        QCOMPARE(int(c.blocks.size()), 2);
        QCOMPARE(c.blocks[0].title, QString("2025 (March 1)"));
        QCOMPARE(c.blocks[1].title, QString("2024"));
    }

    void march1NeverMatchesFeb29()
    {
        QMap<QDate, WeatherDay> e;
        e.insert(QDate(2024, 2, 29), wd(7.0));
        const auto c = History::compareDay(QDate(2028, 3, 1), std::nullopt, e, System::Metric);
        QVERIFY(c.visible);
        QVERIFY(c.blocks.isEmpty());
    }

    void usualComparison()
    {
        QMap<QDate, WeatherDay> e;
        for (int d = 2; d <= 8; ++d)                    // Oct 2..8, seven values of 10 C
            e.insert(QDate(2025, 10, d), wd(10.0));
        const auto today = wd(20.0);
        const auto metric = History::compareDay(QDate(2026, 10, 5), today, e, System::Metric);
        QVERIFY(anyContains(metric, "High is 10 \u00B0C warmer than usual (usual 10 \u00B0C)"));
        // 10 C of difference is 18 F, not 50 F: no +32 on a difference
        const auto imp = History::compareDay(QDate(2026, 10, 5), today, e, System::Imperial);
        QVERIFY(anyContains(imp, "18 \u00B0F warmer than usual"));
        QVERIFY(anyContains(imp, "usual 50 \u00B0F"));
    }

    void usualNeedsEnoughData()
    {
        QMap<QDate, WeatherDay> e;
        e.insert(QDate(2025, 10, 5), wd(10.0));
        e.insert(QDate(2025, 10, 6), wd(10.0));         // only 2 values
        const auto c = History::compareDay(QDate(2026, 10, 5), wd(20.0), e, System::Metric);
        QVERIFY(!anyContains(c, "usual"));
    }

    void warmestColdestTieGoesToNewest()
    {
        QMap<QDate, WeatherDay> e;
        e.insert(QDate(2024, 10, 5), wd(10.0));
        e.insert(QDate(2023, 10, 5), wd(10.0));
        e.insert(QDate(2022, 10, 5), wd(5.0));
        const auto c = History::compareDay(QDate(2026, 10, 5), std::nullopt, e, System::Metric);
        QVERIFY(anyContains(c, "Warmest: 2024"));
        QVERIFY(anyContains(c, "Coldest: 2022"));
    }

    void precipitationFrequency()
    {
        QMap<QDate, WeatherDay> e;
        WeatherDay a = wd(1.0); a.precipType = "Rain"; a.precipMm = 2.0;
        WeatherDay b = wd(1.0); b.precipType = "None"; b.precipMm = 0.0;
        WeatherDay c3 = wd(1.0); c3.precipType = "Rain"; c3.precipMm = 1.0;
        e.insert(QDate(2025, 10, 5), a);
        e.insert(QDate(2024, 10, 5), b);
        e.insert(QDate(2023, 10, 5), c3);
        const auto c = History::compareDay(QDate(2026, 10, 5), std::nullopt, e, System::Metric);
        QVERIFY(anyContains(c, "in 2 of 3 earlier years"));
    }

    void rainAndSnowStaySeparate()
    {
        QMap<QDate, WeatherDay> e;
        WeatherDay r = wd(1.0); r.precipType = "Rain"; r.precipMm = 8.0;
        WeatherDay s = wd(1.0); s.precipType = "Snow"; s.precipMm = 150.0;   // 15 cm
        e.insert(QDate(2024, 10, 5), r);
        e.insert(QDate(2023, 10, 5), s);
        const auto c = History::compareDay(QDate(2026, 10, 5), std::nullopt, e, System::Metric);
        QVERIFY(anyContains(c, "Most rain: 8 mm (2024)"));
        QVERIFY(anyContains(c, "Deepest snow: 15 cm (2023)"));
    }

    void periodHiddenWithoutEarlierData()
    {
        QVERIFY(!History::comparePeriod(QDate(2026, 10, 19), Period::Month, {}, {},
                                        System::Metric).visible);
    }

    void earlierPeriodNeedsFiveDays()
    {
        auto four = daysOf(2025, 10, 1, 4, 10.0, 2.0);
        auto c = History::comparePeriod(QDate(2026, 10, 19), Period::Month, {}, four, System::Metric);
        QVERIFY(c.visible);
        QVERIFY(c.blocks.isEmpty());
        QVERIFY(!c.emptyMessage.isEmpty());

        auto five = daysOf(2025, 10, 1, 5, 10.0, 2.0);
        c = History::comparePeriod(QDate(2026, 10, 19), Period::Month, {}, five, System::Metric);
        QCOMPARE(int(c.blocks.size()), 1);
        QCOMPARE(c.blocks[0].title, QString("2025"));
    }

    void averagesAgainstEarlierYears()
    {
        const auto now = daysOf(2026, 10, 1, 6, 12.0, 4.0);
        const auto before = daysOf(2025, 10, 1, 6, 10.0, 2.0);
        const auto m = History::comparePeriod(QDate(2026, 10, 19), Period::Month, now, before,
                                              System::Metric);
        QVERIFY(periodHas(m, "Average high is 2 \u00B0C warmer than in earlier years "
                             "(they averaged 10 \u00B0C)"));
        // A 2 C difference is 3.6 F, and the old average 10 C is 50 F
        const auto i = History::comparePeriod(QDate(2026, 10, 19), Period::Month, now, before,
                                              System::Imperial);
        QVERIFY(periodHas(i, "3.6 \u00B0F warmer than in earlier years (they averaged 50 \u00B0F)"));
    }

    void bothSidesNeedEnoughPoints()
    {
        const auto now = daysOf(2026, 10, 1, 3, 12.0, 4.0);      // only 3 days this year
        const auto before = daysOf(2025, 10, 1, 6, 10.0, 2.0);
        const auto c = History::comparePeriod(QDate(2026, 10, 19), Period::Month, now, before,
                                              System::Metric);
        QVERIFY(!periodHas(c, "Average high"));
        QCOMPARE(int(c.blocks.size()), 1);                       // the earlier block still shows
    }

    void winterBlockSpansNewYear()
    {
        const auto before = daysOf(2025, 12, 1, 5, 5.0, -2.0);
        const auto c = History::comparePeriod(QDate(2027, 1, 15), Period::Season, {}, before,
                                              System::Metric);
        QCOMPARE(int(c.blocks.size()), 1);
        QCOMPARE(c.blocks[0].title, QString("2025\u201326"));
    }

    void warmestColdestPeriodTieGoesToNewest()
    {
        auto e = daysOf(2025, 10, 1, 5, 10.0, 2.0);
        e.insert(daysOf(2024, 10, 1, 5, 10.0, 2.0));
        e.insert(daysOf(2023, 10, 1, 5, 5.0, 0.0));
        const auto c = History::comparePeriod(QDate(2026, 10, 19), Period::Month, {}, e,
                                              System::Metric);
        QVERIFY(periodHas(c, "Warmest: 2025 (average high 10 \u00B0C)"));
        QVERIFY(periodHas(c, "Coldest: 2023 (average high 5 \u00B0C)"));
    }

    void precipitationShareAgainstEarlierYears()
    {
        QMap<QDate, WeatherDay> now, before;
        for (int d = 1; d <= 5; ++d) {
            WeatherDay a = wd(1.0);
            a.precipType = d <= 2 ? "Rain" : "None";
            a.precipMm = d <= 2 ? 3.0 : 0.0;
            now.insert(QDate(2026, 10, d), a);
            WeatherDay b = wd(1.0);
            b.precipType = d == 1 ? "Rain" : "None";
            b.precipMm = d == 1 ? 2.0 : 0.0;
            before.insert(QDate(2025, 10, d), b);
        }
        const auto c = History::comparePeriod(QDate(2026, 10, 19), Period::Month, now, before,
                                              System::Metric);
        QVERIFY(periodHas(c, "recorded on 40% of days (earlier years: 20%)"));
    }

    void wettestAndSnowiestDayKeepTheirDates()
    {
        auto e = daysOf(2025, 10, 1, 5, 1.0, 0.0);
        for (auto it = e.begin(); it != e.end(); ++it) {
            it->precipType = "Rain";
            it->precipMm = 4.0;
        }
        e[QDate(2025, 10, 3)].precipMm = 12.0;
        e[QDate(2025, 10, 4)].precipType = "Snow";
        e[QDate(2025, 10, 4)].precipMm = 150.0;
        const auto c = History::comparePeriod(QDate(2026, 10, 19), Period::Month, {}, e,
                                              System::Metric);
        QVERIFY(periodHas(c, "Wettest day: 12 mm (October 3, 2025)"));
        QVERIFY(periodHas(c, "Snowiest day: 15 cm (October 4, 2025)"));   // depth, not mm
    }
};

QTEST_APPLESS_MAIN(TestHistory)
#include "tst_history.moc"