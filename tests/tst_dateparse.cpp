#include <QtTest>

#include "WeatherInput.h"

class TestDateParse : public QObject
{
    Q_OBJECT

private slots:
    void accepts_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("iso")            << "2026-10-21";
        QTest::newRow("iso short")      << "2026-1-5";
        QTest::newRow("long")           << "October 21, 2026";
        QTest::newRow("long no comma")  << "October 21 2026";
        QTest::newRow("short month")    << "Oct 21, 2026";
        QTest::newRow("day first")      << "21 October 2026";
        QTest::newRow("lower case")     << "october 21, 2026";
        QTest::newRow("extra spaces")   << "  October   21,   2026 ";
    }
    void accepts()
    {
        QFETCH(QString, text);
        const auto d = WeatherInput::parseDate(text);
        QVERIFY(d.has_value());
        QVERIFY(d->isValid());
    }

    void rejects_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty")        << "";
        QTest::newRow("words")        << "tomorrow";
        QTest::newRow("no such day")  << "2026-02-30";
        QTest::newRow("no year")      << "October 21";
        QTest::newRow("slashes")      << "10/11/2026";   // day or month first? we do not guess
    }
    void rejects()
    {
        QFETCH(QString, text);
        QVERIFY(!WeatherInput::parseDate(text).has_value());
    }

    void exactDate()
    {
        QCOMPARE(*WeatherInput::parseDate("October 21, 2026"), QDate(2026, 10, 21));
        QCOMPARE(*WeatherInput::parseDate("2026-1-5"), QDate(2026, 1, 5));
    }
};

QTEST_APPLESS_MAIN(TestDateParse)
#include "tst_dateparse.moc"
