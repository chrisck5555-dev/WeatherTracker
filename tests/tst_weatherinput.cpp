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
        // K5: spaces, a degree sign and a typographic minus.
        QCOMPARE(WeatherInput::parseNumber("12,5 \u00B0", v), WeatherInput::Parse::Ok);
        QCOMPARE(v, 12.5);
        QCOMPARE(WeatherInput::parseNumber(" \u2212 4 \u00B0C", v), WeatherInput::Parse::Invalid);   // a unit is not a number
        QCOMPARE(WeatherInput::parseNumber("\u22124\u00B0", v), WeatherInput::Parse::Ok);
        QCOMPARE(v, -4.0);
        QCOMPARE(WeatherInput::parseNumber("1 013", v), WeatherInput::Parse::Ok);
        QCOMPARE(v, 1013.0);
        QCOMPARE(WeatherInput::parseNumber("\u00B0", v), WeatherInput::Parse::Blank);
    }

    // D54: the amount's unit follows the type.
    void precipUnit()
    {
        using WeatherInput::precipUnit;
        const auto metric = Units::System::Metric, imperial = Units::System::Imperial;
        QCOMPARE(precipUnit("Snow", metric).unit, QString("cm"));
        QCOMPARE(precipUnit("Rain", metric).unit, QString("mm"));
        QCOMPARE(precipUnit("Hail", metric).unit, QString("mm"));
        QCOMPARE(precipUnit("", metric).unit, QString("mm"));
        QCOMPARE(precipUnit("Snow", imperial).unit, QString("in"));
        QCOMPARE(precipUnit("Rain", imperial).unit, QString("in"));
        QCOMPARE(precipUnit("Snow", metric).spoken, QString("centimetres"));
        QCOMPARE(WeatherInput::amountToMm(2.5, "Snow", metric), 25.0);
        QCOMPARE(WeatherInput::amountToMm(2.5, "Rain", metric), 2.5);
        QCOMPARE(WeatherInput::amountToMm(1.0, "Snow", imperial), 25.4);
        QCOMPARE(WeatherInput::mmToAmount(25.0, "Snow", metric), 2.5);
        QCOMPARE(WeatherInput::mmToAmount(25.4, "Rain", imperial), 1.0);
        QVERIFY(WeatherInput::sameAmountUnit("Rain", "Hail"));
        QVERIFY(!WeatherInput::sameAmountUnit("Rain", "Snow"));
        QVERIFY(WeatherInput::sameAmountUnit("Snow", "Snow"));
    }

    // CF4, D53: the editor says "going to"; what is saved stays "from".
    void windGoingTo()
    {
        const QStringList sides{"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        const QStringList opposite{"S", "SW", "W", "NW", "N", "NE", "E", "SE"};
        for (int i = 0; i < sides.size(); ++i) {
            QCOMPARE(WeatherInput::windGoingToFrom(sides[i]), opposite[i]);   // loading
            QCOMPARE(WeatherInput::windFromGoingTo(opposite[i]), sides[i]);   // saving
            QCOMPARE(WeatherInput::windFromGoingTo(WeatherInput::windGoingToFrom(sides[i])), sides[i]);
        }
        QCOMPARE(WeatherInput::windGoingToFrom(""), QString());               // not recorded stays so
        QCOMPARE(WeatherInput::windFromGoingTo(""), QString());
    }

    void spokenUnits()
    {
        QCOMPARE(WeatherInput::spokenUnit("\u00B0C"), QString("degrees Celsius"));
        QCOMPARE(WeatherInput::spokenUnit("km/h"), QString("kilometres per hour"));
        QCOMPARE(WeatherInput::spokenUnit("hPa"), QString("hectopascals"));
        QCOMPARE(WeatherInput::spokenUnit("%"), QString("percent"));
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