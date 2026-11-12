#include <QtTest>

#include "Format.h"

class TestFormat : public QObject
{
    Q_OBJECT

private slots:
    void number_data()
    {
        QTest::addColumn<double>("value");
        QTest::addColumn<int>("decimals");
        QTest::addColumn<QString>("expected");
        QTest::newRow("whole")         << 21.0  << 1 << QString("21");
        QTest::newRow("decimal")       << 21.34 << 1 << QString("21.3");
        QTest::newRow("negative")      << -4.7  << 1 << QString("-4.7");
        QTest::newRow("negative zero") << -0.04 << 1 << QString("0");
        QTest::newRow("rounds up")     << 2.96  << 1 << QString("3");
        QTest::newRow("no decimals")   << 65.4  << 0 << QString("65");
    }
    void number()
    {
        QFETCH(double, value);
        QFETCH(int, decimals);
        QFETCH(QString, expected);
        QCOMPARE(Format::number(value, decimals), expected);
    }

    void notRecorded()
    {
        QCOMPARE(Format::temperature(std::nullopt), QString("Not recorded"));
        QCOMPARE(Format::text(QString()), QString("Not recorded"));
        QCOMPARE(Format::temperature(0.0), QString("0 \u00B0C"));   // zero is a real value
    }

    void precipitation()
    {
        QCOMPARE(Format::precipitation("Rain", 3.2), QString("Rain, 3.2 mm"));
        QCOMPARE(Format::precipitation("Snow", 500.0), QString("Snow, 50 cm"));
        QCOMPARE(Format::precipitation("None", 0.0), QString("None"));
        QCOMPARE(Format::precipitation("", std::nullopt), QString("Not recorded"));
        QCOMPARE(Format::precipitation("Rain", std::nullopt), QString("Rain"));
        QCOMPARE(Format::precipitation("", 2.0), QString("2 mm"));
    }
    // CF4: the saved side the wind comes from becomes the side it goes to.
    void windGoingTo()
    {
        QCOMPARE(Format::windGoingTo("N"), QString("S"));
        QCOMPARE(Format::windGoingTo("NW"), QString("SE"));
        QCOMPARE(Format::windGoingTo("E"), QString("W"));
        QCOMPARE(Format::windGoingTo(""), QString());
        QCOMPARE(Format::wind(12.0, "NW"), QString("12 km/h going to the SE"));
        QCOMPARE(Format::wind(std::nullopt, "S"), QString("Going to the N"));
        QCOMPARE(Format::wind(5.5, ""), QString("5.5 km/h"));
        QCOMPARE(Format::wind(std::nullopt, ""), QString("Not recorded"));
    }

    void describesADayForScreenReaders()
    {
        WeatherDay w;
        w.cloud = "Overcast";
        w.precipType = "Rain";
        w.precipMm = 4;
        w.highC = 8.5;
        w.lowC = 1;
        QCOMPARE(Format::describeDay(QDate(2026, 10, 27), w, true, false),
                 QString("Tuesday, October 27, 2026. Overcast, rain, 4 mm, high 8.5 \u00B0C, "
                         "low 1 \u00B0C. Has a note."));
        QCOMPARE(Format::describeDay(QDate(2026, 10, 28), std::nullopt, false, false),
                 QString("Wednesday, October 28, 2026. Nothing recorded."));
        QCOMPARE(Format::describeDay(QDate(2026, 10, 29), WeatherDay{}, false, true),
                 QString("Thursday, October 29, 2026. Weather recorded. Has an event."));
    }
};

QTEST_APPLESS_MAIN(TestFormat)
#include "tst_format.moc"