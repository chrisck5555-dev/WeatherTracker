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
};

QTEST_APPLESS_MAIN(TestFormat)
#include "tst_format.moc"