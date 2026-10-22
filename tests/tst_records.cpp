#include <QtTest>

#include "Records.h"

static WeatherDay day(std::optional<double> hi, std::optional<double> lo,
                      std::optional<double> precipMm, const QString &type = QString())
{
    WeatherDay d;
    d.highC = hi;
    d.lowC = lo;
    d.precipMm = precipMm;
    d.precipType = type;
    return d;
}

static QString valueOf(const QList<Records::Item> &rows, const QString &label)
{
    for (const auto &r : rows)
        if (r.label == label)
            return r.value;
    return "(missing)";
}

class TestRecords : public QObject
{
    Q_OBJECT

private slots:
    void emptyGivesNoRows()
    {
        QVERIFY(Records::build({}, Units::System::Metric, QDate(2026, 7, 31)).isEmpty());
    }

    void hottestAndColdest()
    {
        QMap<QDate, WeatherDay> days;
        days[QDate(2026, 7, 1)] = day(20, 10, 0, "None");
        days[QDate(2026, 7, 2)] = day(31.5, 12, 0, "None");
        days[QDate(2026, 7, 3)] = day(25, -36, 0, "None");
        const auto rows = Records::build(days, Units::System::Metric, QDate(2026, 8, 1));
        QVERIFY(valueOf(rows, "Hottest day").startsWith("31.5"));
        QVERIFY(valueOf(rows, "Coldest day").startsWith("-36"));
        QCOMPARE(valueOf(rows, "Days below -35 °C"), QString("1 of 3 days with a recorded low"));
        QCOMPARE(valueOf(rows, "Days above 25 °C"), QString("1 of 3 days with a recorded high"));
    }

    void dryStreakStopsAtUnknownDay()
    {
        QMap<QDate, WeatherDay> days;                       // dry, dry, (unknown), dry, dry, dry
        days[QDate(2026, 7, 1)] = day(20, 10, 0, "None");
        days[QDate(2026, 7, 2)] = day(20, 10, 0, "None");
        days[QDate(2026, 7, 4)] = day(20, 10, 0, "None");
        days[QDate(2026, 7, 5)] = day(20, 10, 0, "None");
        days[QDate(2026, 7, 6)] = day(20, 10, 0, "None");
        const auto rows = Records::build(days, Units::System::Metric, QDate(2026, 9, 1));
        QVERIFY(valueOf(rows, "Longest dry streak").startsWith("3 dry days"));
    }

    void currentStreakOnlyWhenPeriodReachesToday()
    {
        QMap<QDate, WeatherDay> days;
        days[QDate(2026, 7, 1)] = day(20, 10, 2, "Rain");
        auto rows = Records::build(days, Units::System::Metric, QDate(2026, 9, 1));
        QCOMPARE(valueOf(rows, "Current streak"), QString("(missing)"));
        rows = Records::build(days, Units::System::Metric, QDate(2026, 7, 2));
        QVERIFY(valueOf(rows, "Current streak").startsWith("1 wet day"));
    }

    void snowIsNotWettest()
    {
        QMap<QDate, WeatherDay> days;
        days[QDate(2026, 1, 1)] = day(-5, -10, 80, "Snow");
        days[QDate(2026, 1, 2)] = day(2, -1, 5, "Rain");
        const auto rows = Records::build(days, Units::System::Metric, QDate(2026, 3, 1));
        QVERIFY(valueOf(rows, "Wettest day (rain)").startsWith("5 mm"));
        QVERIFY(valueOf(rows, "Snowiest day").startsWith("8 cm"));
    }

    void imperialLimits()
    {
        QMap<QDate, WeatherDay> days;
        days[QDate(2026, 1, 1)] = day(20, 10, 0, "None");
        const auto rows = Records::build(days, Units::System::Imperial, QDate(2026, 3, 1));
        QCOMPARE(valueOf(rows, "Days above 77 °F"), QString("0 of 1 days with a recorded high"));
        QCOMPARE(valueOf(rows, "Days below -31 °F"), QString("0 of 1 days with a recorded low"));
    }
};

QTEST_APPLESS_MAIN(TestRecords)
#include "tst_records.moc"
