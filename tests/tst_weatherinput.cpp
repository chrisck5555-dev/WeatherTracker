#include <QtTest>

#include "WeatherInput.h"

class TestWeatherInput : public QObject
{
    Q_OBJECT

private slots:
    void parse()
    {
        double v = 0;
        QCOMPARE(WeatherInput::parseNumber("", v), WeatherInput::Parse::Blank);
        QCOMPARE(WeatherInput::parseNumber("   ", v), WeatherInput::Parse::Blank);
        QCOMPARE(WeatherInput::parseNumber("12.5", v), WeatherInput::Parse::Ok);
        QCOMPARE(v, 12.5);
        QCOMPARE(WeatherInput::parseNumber("12,5", v), WeatherInput::Parse::Ok);
        QCOMPARE(v, 12.5);
        QCOMPARE(WeatherInput::parseNumber("-4", v), WeatherInput::Parse::Ok);
        QCOMPARE(WeatherInput::parseNumber("abc", v), WeatherInput::Parse::Invalid);
        QCOMPARE(WeatherInput::parseNumber("1.2.3", v), WeatherInput::Parse::Invalid);
        QCOMPARE(WeatherInput::parseNumber("inf", v), WeatherInput::Parse::Invalid);
        QCOMPARE(WeatherInput::parseNumber("nan", v), WeatherInput::Parse::Invalid);
    }

    void ranges()
    {
        std::optional<double> v;
        QVERIFY(!WeatherInput::validateNumber("-5", std::nullopt, std::nullopt, v).isEmpty() == false);
        QCOMPARE(*v, -5.0);                                           // no limits: negative is fine
        QVERIFY(!WeatherInput::validateNumber("-1", 0.0, std::nullopt, v).isEmpty());   // wind < 0
        QVERIFY(!v.has_value());
        QVERIFY(WeatherInput::validateNumber("0", 0.0, std::nullopt, v).isEmpty());     // 0 is allowed
        QVERIFY(!WeatherInput::validateNumber("101", 0.0, 100.0, v).isEmpty());          // humidity
        QVERIFY(WeatherInput::validateNumber("100", 0.0, 100.0, v).isEmpty());
        QVERIFY(WeatherInput::validateNumber("", 0.0, 100.0, v).isEmpty());              // blank is OK
        QVERIFY(!v.has_value());                                                          // ...and unset
        QVERIFY(!WeatherInput::validateNumber("abc", std::nullopt, std::nullopt, v).isEmpty());
    }

    void precipPair()
    {
        using WeatherInput::checkPrecipPair;
        QVERIFY(checkPrecipPair("", true).typeError.isEmpty());
        QVERIFY(checkPrecipPair("", true).amountError.isEmpty());          // both blank: fine
        QVERIFY(!checkPrecipPair("", false).typeError.isEmpty());          // amount, no type
        QVERIFY(!checkPrecipPair("Rain", true).amountError.isEmpty());     // type, no amount
        QVERIFY(checkPrecipPair("Rain", false).amountError.isEmpty());
        QVERIFY(checkPrecipPair("None", true).amountError.isEmpty());      // None needs no amount
    }
};

QTEST_APPLESS_MAIN(TestWeatherInput)
#include "tst_weatherinput.moc"