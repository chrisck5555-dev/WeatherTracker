#include <QtTest>

#include "Units.h"

using Units::System;

class TestUnits : public QObject
{
    Q_OBJECT

private slots:
    void temperature()
    {
        QCOMPARE(Units::cToDisplay(0.0, System::Imperial), 32.0);
        QCOMPARE(Units::cToDisplay(100.0, System::Imperial), 212.0);
        QCOMPARE(Units::cToDisplay(-40.0, System::Imperial), -40.0);
        QCOMPARE(Units::cToDisplay(21.5, System::Metric), 21.5);
        QCOMPARE(Units::displayToC(32.0, System::Imperial), 0.0);
    }

    void deltaHasNoOffset()
    {
        QCOMPARE(Units::deltaCToDisplay(10.0, System::Imperial), 18.0);
        QCOMPARE(Units::deltaCToDisplay(0.0, System::Imperial), 0.0);     // not 32
        QCOMPARE(Units::deltaCToDisplay(3.0, System::Metric), 3.0);
    }

    void roundTrips()
    {
        for (double c = -40.0; c <= 45.0; c += 0.1) {
            const double back = Units::displayToC(Units::cToDisplay(c, System::Imperial),
                                                  System::Imperial);
            QVERIFY(qAbs(back - c) < 1e-9);       // close, but not promised to be identical
        }
    }

    void distancesAndPressure()
    {
        QVERIFY(qAbs(Units::kmhToDisplay(100.0, System::Imperial) - 62.1371) < 1e-3);
        QVERIFY(qAbs(Units::displayToKmh(60.0, System::Imperial) - 96.56064) < 1e-6);
        QVERIFY(qAbs(Units::hpaToDisplay(1013.25, System::Imperial) - 29.9213) < 1e-3);
        QCOMPARE(Units::mmToDisplay(25.4, System::Imperial), 1.0);
    }

    void snow()
    {
        QCOMPARE(Units::snowMmToDisplay(500.0, System::Metric), 50.0);    // 500 mm -> 50 cm
        QCOMPARE(Units::snowMmToDisplay(254.0, System::Imperial), 10.0);  // 254 mm -> 10 in
        QCOMPARE(Units::displayToSnowMm(50.0, System::Metric), 500.0);
    }

    void settingText()
    {
        QCOMPARE(Units::fromSetting("imperial"), System::Imperial);
        QCOMPARE(Units::fromSetting("metric"), System::Metric);
        QCOMPARE(Units::fromSetting(""), System::Metric);                 // unknown -> Metric
        QCOMPARE(Units::toSetting(System::Imperial), QString("imperial"));
    }
};

QTEST_APPLESS_MAIN(TestUnits)
#include "tst_units.moc"