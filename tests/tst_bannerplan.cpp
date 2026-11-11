#include <QtTest>

#include "BannerPlan.h"

using BannerPlan::Sky;

class TestBannerPlan : public QObject
{
    Q_OBJECT

    static QStringList names(const QList<BannerPlan::Square> &squares)
    {
        QStringList out;
        for (const auto &s : squares) {
            QString g;
            for (const auto &p : s.ground)
                g += QString("%1%2%3").arg(p.h0).arg(p.h1).arg(p.concaveUp ? 'u' : 'd');
            out << s.sky + "/" + g;
        }
        return out;
    }

private slots:
    // The same day always gets the same banner, and a wider window only adds squares.
    void sameDaySameBanner()
    {
        const QDate d(2026, 11, 10);
        QCOMPARE(names(BannerPlan::plan(d, Sky::Rain, 12)), names(BannerPlan::plan(d, Sky::Rain, 12)));
        QCOMPARE(names(BannerPlan::plan(d, Sky::Rain, 30)).mid(0, 8),
                 names(BannerPlan::plan(d, Sky::Rain, 8)));
    }

    // Every day of a year looks different from every other.
    void everyDayDifferent()
    {
        QSet<QString> seen;
        for (QDate d(2026, 1, 1); d.year() == 2026; d = d.addDays(1))
            seen.insert(names(BannerPlan::plan(d, Sky::PartlyCloudy, 12)).join(','));
        QCOMPARE(int(seen.size()), 365);
    }

    // The hills make one smooth line: each piece starts at the last one's height and slope,
    // within 0-2, one step at most.
    void groundJoins()
    {
        for (QDate d(2026, 1, 1); d.year() == 2026; d = d.addDays(1)) {
            int last = -1;
            double slope = 0;
            for (const auto &s : BannerPlan::plan(d, Sky::Overcast, 30))
                for (const auto &p : s.ground) {
                    QVERIFY(p.h0 >= 0 && p.h0 <= 2 && p.h1 >= 0 && p.h1 <= 2);
                    QVERIFY(std::abs(p.h0 - p.h1) <= 1);
                    if (last >= 0) {
                        QCOMPARE(p.h0, last);
                        QVERIFY(std::abs(BannerPlan::edgeSlopes(p).left - slope) <= BannerPlan::kSmoothJoin);
                    }
                    last = p.h1;
                    slope = BannerPlan::edgeSlopes(p).right;
                }
        }
    }

    // Every way a piece can end has a smooth way to go on, so the line never breaks.
    void alwaysAWayOn()
    {
        for (int h0 = 0; h0 <= 2; ++h0)
            for (int h1 = std::max(0, h0 - 1); h1 <= std::min(2, h0 + 1); ++h1)
                for (bool up : {false, true}) {
                    const double end = BannerPlan::edgeSlopes({h0, h1, up}).right;
                    int ways = 0;
                    for (int next = std::max(0, h1 - 1); next <= std::min(2, h1 + 1); ++next)
                        for (bool up2 : {false, true})
                            if (std::abs(BannerPlan::edgeSlopes({h1, next, up2}).left - end)
                                <= BannerPlan::kSmoothJoin)
                                ++ways;
                    QVERIFY2(ways > 0, qPrintable(QString("%1_%2 %3").arg(h0).arg(h1).arg(up)));
                }
    }

    // One sun (or moon) where the sky has one, always among the first squares.
    void oneSun()
    {
        auto suns = [](const QList<BannerPlan::Square> &squares, bool behindCloud) {
            QList<int> at;
            for (int i = 0; i < squares.size(); ++i)
                if (squares[i].sky.startsWith("sun") && squares[i].sky.contains("cloud") == behindCloud)
                    at << i;
            return at;
        };
        for (QDate d(2026, 1, 1); d.year() == 2026; d = d.addDays(3)) {
            for (Sky sky : {Sky::Clear, Sky::MostlyClear, Sky::PartlyCloudy}) {
                const auto at = suns(BannerPlan::plan(d, sky, 25), false);
                QCOMPARE(int(at.size()), 1);
                QVERIFY(at.first() >= BannerPlan::kBodyFirst);
                QVERIFY(at.first() < BannerPlan::kBodyFirst + BannerPlan::kBodySpan);
            }
            const auto cloudy = BannerPlan::plan(d, Sky::MostlyCloudy, 25);
            QCOMPARE(int(suns(cloudy, true).size()), 1);
            QCOMPARE(int(suns(cloudy, false).size()), 0);
            for (Sky sky : {Sky::Overcast, Sky::Rain, Sky::Snow, Sky::Plain})
                for (const auto &s : BannerPlan::plan(d, sky, 25))
                    QVERIFY(!s.sky.startsWith("sun"));
        }
    }

    // Overcast is clouds in every square; rain and snow use only their own squares.
    void skiesMatchTheWeather()
    {
        const QDate d(2026, 3, 14);
        for (const auto &s : BannerPlan::plan(d, Sky::Overcast, 25))
            QVERIFY(s.sky.startsWith("cloud-") && !s.sky.contains('_'));
        for (const auto &s : BannerPlan::plan(d, Sky::Rain, 25))
            QVERIFY(s.sky.endsWith("rain"));
        for (const auto &s : BannerPlan::plan(d, Sky::Snow, 25))
            QVERIFY(s.sky.endsWith("snow"));
        for (const auto &s : BannerPlan::plan(d, Sky::Clear, 25))
            QVERIFY(s.sky == "clear" || s.sky == "sun");
        for (const auto &s : BannerPlan::plan(d, Sky::Plain, 25))
            QCOMPARE(s.sky, QString("clear"));
        // Neighbours differ, except clear sky.
        const auto rain = BannerPlan::plan(d, Sky::Rain, 25);
        for (int i = 1; i < rain.size(); ++i)
            QVERIFY(rain[i].sky != rain[i - 1].sky);
    }

    // IL1: precipitation first, then cloud cover; anything else gets the plain banner.
    void skyFromTheRecord()
    {
        QVERIFY(BannerPlan::skyFor("Snow", "Clear") == Sky::Snow);
        QVERIFY(BannerPlan::skyFor("Drizzle", "") == Sky::Rain);
        QVERIFY(BannerPlan::skyFor("Hail", "Overcast") == Sky::Plain);
        QVERIFY(BannerPlan::skyFor("None", "Mostly cloudy") == Sky::MostlyCloudy);
        QVERIFY(BannerPlan::skyFor("", "Partly cloudy") == Sky::PartlyCloudy);
        QVERIFY(BannerPlan::skyFor("", "") == Sky::Plain);
        QCOMPARE(BannerPlan::skyWords(Sky::MostlyClear), QString("mostly clear"));
    }
};

QTEST_APPLESS_MAIN(TestBannerPlan)
#include "tst_bannerplan.moc"
