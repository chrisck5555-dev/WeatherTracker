#include <QtTest>

#include "PeriodMath.h"

class TestPeriodMath : public QObject
{
    Q_OBJECT

private slots:
    // --- Weeks start on Sunday ---
    void weekStart_data()
    {
        QTest::addColumn<QDate>("input");
        QTest::addColumn<QDate>("expected");
        QTest::newRow("Monday")   << QDate(2026, 10, 5)  << QDate(2026, 10, 4);
        QTest::newRow("Sunday")   << QDate(2026, 10, 4)  << QDate(2026, 10, 4);
        QTest::newRow("Saturday") << QDate(2026, 10, 10) << QDate(2026, 10, 4);
        QTest::newRow("next Sun") << QDate(2026, 10, 11) << QDate(2026, 10, 11);
        QTest::newRow("year end") << QDate(2027, 1, 1)   << QDate(2026, 12, 27);
    }
    void weekStart()
    {
        QFETCH(QDate, input);
        QFETCH(QDate, expected);
        QCOMPARE(PeriodMath::startOf(input, Period::Week), expected);
    }

    // --- Meteorological seasons ---
    void seasonStart_data()
    {
        QTest::addColumn<QDate>("input");
        QTest::addColumn<QDate>("expected");
        QTest::newRow("Dec 1")      << QDate(2026, 12, 1)  << QDate(2026, 12, 1);
        QTest::newRow("Jan")        << QDate(2027, 1, 15)  << QDate(2026, 12, 1);
        QTest::newRow("Feb 28")     << QDate(2027, 2, 28)  << QDate(2026, 12, 1);
        QTest::newRow("Mar 1")      << QDate(2027, 3, 1)   << QDate(2027, 3, 1);
        QTest::newRow("May 31")     << QDate(2027, 5, 31)  << QDate(2027, 3, 1);
        QTest::newRow("Jun 1")      << QDate(2027, 6, 1)   << QDate(2027, 6, 1);
        QTest::newRow("Aug 31")     << QDate(2027, 8, 31)  << QDate(2027, 6, 1);
        QTest::newRow("Sep 1")      << QDate(2027, 9, 1)   << QDate(2027, 9, 1);
        QTest::newRow("Nov 30")     << QDate(2026, 11, 30) << QDate(2026, 9, 1);
    }
    void seasonStart()
    {
        QFETCH(QDate, input);
        QFETCH(QDate, expected);
        QCOMPARE(PeriodMath::startOf(input, Period::Season), expected);
    }

    void winterEndsInFebruary()
    {
        QCOMPARE(PeriodMath::endOf(QDate(2027, 1, 15), Period::Season), QDate(2027, 2, 28));
        QCOMPARE(PeriodMath::endOf(QDate(2028, 1, 15), Period::Season), QDate(2028, 2, 29)); // leap
    }

    void endOfOtherPeriods()
    {
        QCOMPARE(PeriodMath::endOf(QDate(2026, 10, 5), Period::Week),  QDate(2026, 10, 10));
        QCOMPARE(PeriodMath::endOf(QDate(2026, 2, 10), Period::Month), QDate(2026, 2, 28));
        QCOMPARE(PeriodMath::endOf(QDate(2028, 2, 10), Period::Month), QDate(2028, 2, 29));
        QCOMPARE(PeriodMath::endOf(QDate(2026, 6, 1),  Period::Year),  QDate(2026, 12, 31));
    }

    // --- Stepping ---
    void stepAcrossBoundaries()
    {
        QCOMPARE(PeriodMath::step(QDate(2026, 12, 31), Period::Day, +1), QDate(2027, 1, 1));
        QCOMPARE(PeriodMath::step(QDate(2027, 3, 1),   Period::Day, -1), QDate(2027, 2, 28));
        QCOMPARE(PeriodMath::step(QDate(2026, 10, 5),  Period::Week, +1), QDate(2026, 10, 12));
        QCOMPARE(PeriodMath::step(QDate(2027, 1, 31),  Period::Month, +1), QDate(2027, 2, 28));
        QCOMPARE(PeriodMath::step(QDate(2028, 1, 31),  Period::Month, +1), QDate(2028, 2, 29));
        QCOMPARE(PeriodMath::step(QDate(2026, 11, 30), Period::Season, +1), QDate(2027, 2, 28));
    }

    void seasonStepAlwaysChangesSeason()
    {
        // Stepping a season from any day must land in the next season's range.
        for (QDate d(2026, 1, 1); d.year() < 2029; d = d.addDays(1)) {
            const QDate next = PeriodMath::step(d, Period::Season, +1);
            QCOMPARE(PeriodMath::startOf(next, Period::Season),
                     PeriodMath::endOf(d, Period::Season).addDays(1));
        }
    }

    // --- Titles ---
    void titles()
    {
        QCOMPARE(PeriodMath::title(QDate(2027, 1, 15), Period::Season),
                 QString("Winter 2026\u201327"));
        QCOMPARE(PeriodMath::title(QDate(2026, 7, 4), Period::Season), QString("Summer 2026"));
        QCOMPARE(PeriodMath::title(QDate(2026, 10, 5), Period::Week),
                 QString("Week of October 4, 2026"));
        QCOMPARE(PeriodMath::title(QDate(2026, 10, 5), Period::Month), QString("October 2026"));
    }

    // --- Month grid ---
    void monthGrid()
    {
        QCOMPARE(PeriodMath::monthGridStart(2026, 10), QDate(2026, 9, 27)); // Oct 1 is a Thursday
        QCOMPARE(PeriodMath::monthGridStart(2026, 2),  QDate(2026, 2, 1));  // Feb 1 is a Sunday
        for (int m = 1; m <= 12; ++m) {                                     // always fits 42 cells
            const QDate s = PeriodMath::monthGridStart(2026, m);
            QCOMPARE(s.dayOfWeek(), 7);
            QVERIFY(s.addDays(41) >= QDate(2026, m, 1).addMonths(1).addDays(-1));
        }
    }

    // --- Year view: keep the day where valid ---
    void withMonthClamps()
    {
        QCOMPARE(PeriodMath::withMonth(QDate(2027, 1, 31), 2),  QDate(2027, 2, 28));
        QCOMPARE(PeriodMath::withMonth(QDate(2028, 1, 31), 2),  QDate(2028, 2, 29));
        QCOMPARE(PeriodMath::withMonth(QDate(2026, 1, 15), 10), QDate(2026, 10, 15));
    }

    // --- Feb 29 rule ---
    void sameDate_data()
    {
        QTest::addColumn<QDate>("input");
        QTest::addColumn<int>("year");
        QTest::addColumn<QDate>("expected");
        QTest::addColumn<bool>("usedMarch1");
        QTest::newRow("Feb 29, leap year")   << QDate(2028, 2, 29) << 2024 << QDate(2024, 2, 29) << false;
        QTest::newRow("Feb 29, normal year") << QDate(2028, 2, 29) << 2025 << QDate(2025, 3, 1)  << true;
        QTest::newRow("Mar 1 stays Mar 1")   << QDate(2028, 3, 1)  << 2025 << QDate(2025, 3, 1)  << false;
        QTest::newRow("ordinary date")       << QDate(2026, 10, 5) << 2020 << QDate(2020, 10, 5) << false;
    }
    void sameDate()
    {
        QFETCH(QDate, input);
        QFETCH(int, year);
        QFETCH(QDate, expected);
        QFETCH(bool, usedMarch1);
        const auto r = PeriodMath::sameDateInYear(input, year);
        QCOMPARE(r.date, expected);
        QCOMPARE(r.usedMarch1, usedMarch1);
    }
};

QTEST_APPLESS_MAIN(TestPeriodMath)
#include "tst_periodmath.moc"