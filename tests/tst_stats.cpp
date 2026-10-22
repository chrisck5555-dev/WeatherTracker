#include <QtTest>

#include "Stats.h"

static WeatherDay day(std::optional<double> hi = std::nullopt, std::optional<double> lo = std::nullopt)
{
    WeatherDay d;
    d.highC = hi;
    d.lowC = lo;
    return d;
}

class TestStats : public QObject
{
    Q_OBJECT

private slots:
    // --- Compass ---
    void compassSnapping_data()
    {
        QTest::addColumn<double>("degrees");
        QTest::addColumn<QString>("expected");
        QTest::newRow("0")        << 0.0    << "N";
        QTest::newRow("22.4")     << 22.4   << "N";
        QTest::newRow("22.5 tie") << 22.5   << "NE";    // exact tie goes clockwise
        QTest::newRow("45")       << 45.0   << "NE";
        QTest::newRow("337.4")    << 337.4  << "NW";
        QTest::newRow("337.5")    << 337.5  << "N";     // tie between NW and N: clockwise = N
        QTest::newRow("360")      << 360.0  << "N";
        QTest::newRow("-10")      << -10.0  << "N";
        QTest::newRow("-30")      << -30.0  << "NW";
    }
    void compassSnapping()
    {
        QFETCH(double, degrees);
        QFETCH(QString, expected);
        QCOMPARE(Stats::compassName(Stats::compassIndex(degrees)), expected);
    }

    // --- Wind direction average ---
    void windAverage()
    {
        QCOMPARE(Stats::averageWindDirection({}), QString());
        QCOMPARE(Stats::averageWindDirection({"W"}), QString("W"));
        QCOMPARE(Stats::averageWindDirection({"W", "W", "W"}), QString("W"));
        // 350 and 10 degrees average to north, NOT south: NW + NE -> N
        QCOMPARE(Stats::averageWindDirection({"NW", "NE"}), QString("N"));
        // Opposite winds cancel out
        QCOMPARE(Stats::averageWindDirection({"N", "S"}), QString("Variable"));
        QCOMPARE(Stats::averageWindDirection({"E", "W", "N", "S"}), QString("Variable"));
        // Mostly west with one east: still west
        QCOMPARE(Stats::averageWindDirection({"W", "W", "W", "E"}), QString("W"));
        // Unknown text is ignored
        QCOMPARE(Stats::averageWindDirection({"W", "banana"}), QString("W"));
    }

    void windTieGoesClockwise()
    {
        // N (0 deg) and E (90 deg) average to exactly 45 deg = NE, no tie. Use three
        // points whose mean lands on a half-way angle: N, N, E -> atan2(1,2) = 26.57 -> NE
        QCOMPARE(Stats::averageWindDirection({"N", "N", "E"}), QString("NE"));
        QCOMPARE(Stats::averageWindDirection({"N", "N", "N", "E"}), QString("N"));
    }

    // --- Modes ---
    void cloudModes()
    {
        QCOMPARE(Stats::modes({"Clear", "Overcast", "Clear"}), QStringList{"Clear"});
        QCOMPARE(Stats::modes({"Clear", "Partly cloudy"}),
                 (QStringList{"Clear", "Partly cloudy"}));                // tie: all listed
        QCOMPARE(Stats::modes({"Overcast", "Clear", "Clear", "Overcast"}),
                 (QStringList{"Overcast", "Clear"}));                     // first-seen order
        QVERIFY(Stats::modes({}).isEmpty());
        QVERIFY(Stats::modes({"", ""}).isEmpty());                        // blanks ignored
    }

    // --- Summary ---
    void emptyList()
    {
        const auto s = Stats::summarize({});
        QCOMPARE(s.daysRecorded, 0);
        QVERIFY(!s.high.value.has_value());
        QCOMPARE(s.high.count, 0);
        QVERIFY(!s.precipTotalMm.has_value());
        QVERIFY(s.windDirection.isEmpty());
        QVERIFY(s.cloudModes.isEmpty());
    }

    void blanksAreSkippedNotZero()
    {
        // Highs of 10 and 20, plus a day with no high. Average is 15, from 2 days.
        const auto s = Stats::summarize({day(10.0, 0.0), day(20.0, 4.0), day(std::nullopt, 2.0)});
        QCOMPARE(s.daysRecorded, 3);
        QCOMPARE(*s.high.value, 15.0);
        QCOMPARE(s.high.count, 2);
        QCOMPARE(*s.low.value, 2.0);
        QCOMPARE(s.low.count, 3);
    }

    void meanNeedsBothHighAndLow()
    {
        // Only the first day has both: mean = 5. The others must not drag it around.
        const auto s = Stats::summarize({day(10.0, 0.0), day(30.0, std::nullopt), day(std::nullopt, -50.0)});
        QCOMPARE(*s.mean.value, 5.0);
        QCOMPARE(s.mean.count, 1);
    }

    void allBlankDayIsNotRecorded()
    {
        const auto s = Stats::summarize({WeatherDay{}, day(5.0, 1.0)});
        QCOMPARE(s.daysRecorded, 1);
    }

    void precipitation()
    {
        WeatherDay none;    none.precipType = "None";  none.precipMm = 0.0;
        WeatherDay rain;    rain.precipType = "Rain";  rain.precipMm = 6.0;
        WeatherDay rain2;   rain2.precipType = "Rain"; rain2.precipMm = 2.0;
        WeatherDay snow;    snow.precipType = "Snow";  snow.precipMm = 100.0;   // 10 cm depth
        WeatherDay missing = day(3.0, 1.0);                                       // no precip figure

        const auto s = Stats::summarize({none, rain, rain2, snow, missing});
        QCOMPARE(s.daysRecorded, 5);
        QCOMPARE(s.precipDays, 4);                       // 'missing' has no figure
        QCOMPARE(*s.precipTotalMm, 108.0);
        QCOMPARE(*s.precipPerDayMm, 27.0);               // 108 / 4, not / 5

        QCOMPARE(int(s.precipByType.size()), 3);         // None, Rain, Snow (display order)
        QCOMPARE(s.precipByType[0].type, QString("None"));
        QCOMPARE(s.precipByType[0].days, 1);
        QCOMPARE(s.precipByType[1].type, QString("Rain"));
        QCOMPARE(s.precipByType[1].days, 2);
        QCOMPARE(s.precipByType[1].totalMm, 8.0);
        QCOMPARE(s.precipByType[2].type, QString("Snow"));
        QCOMPARE(s.precipByType[2].totalMm, 100.0);      // kept separate from rain
    }

    void noPrecipFiguresMeansUnset()
    {
        const auto s = Stats::summarize({day(5.0, 1.0)});
        QVERIFY(!s.precipTotalMm.has_value());
        QVERIFY(!s.precipPerDayMm.has_value());
        QCOMPARE(s.precipDays, 0);
    }

    void windAndCloudInSummary()
    {
        WeatherDay a; a.windDir = "W"; a.cloud = "Clear";
        WeatherDay b; b.windDir = "W"; b.cloud = "Overcast";
        WeatherDay c; c.windDir = "W"; c.cloud = "Clear";
        const auto s = Stats::summarize({a, b, c});
        QCOMPARE(s.windDirection, QString("W"));
        QCOMPARE(s.cloudModes, QStringList{"Clear"});
    }
};

QTEST_APPLESS_MAIN(TestStats)
#include "tst_stats.moc"