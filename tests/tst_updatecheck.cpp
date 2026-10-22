#include <QtTest>

#include "UpdateCheck.h"

class TestUpdateCheck : public QObject
{
    Q_OBJECT

private slots:
    void newer_data()
    {
        QTest::addColumn<QString>("latest");
        QTest::addColumn<QString>("current");
        QTest::addColumn<bool>("expected");
        QTest::newRow("patch")       << "v1.0.1" << "1.0.0" << true;
        QTest::newRow("minor")       << "1.1.0"  << "1.0.9" << true;
        QTest::newRow("ten > nine")  << "v1.10.0" << "1.9.0" << true;
        QTest::newRow("same")        << "v1.0.0" << "1.0.0" << false;
        QTest::newRow("older")       << "v0.9.0" << "1.0.0" << false;
        QTest::newRow("short form")  << "1.2"    << "1.2.0" << false;
        QTest::newRow("suffix")      << "v1.2.0-beta" << "1.1.0" << true;
        QTest::newRow("junk")        << "latest" << "1.0.0" << false;
    }
    void newer()
    {
        QFETCH(QString, latest);
        QFETCH(QString, current);
        QFETCH(bool, expected);
        QCOMPARE(UpdateCheck::isNewer(latest, current), expected);
    }

    void parsesARelease()
    {
        const auto r = UpdateCheck::parseRelease(
            R"({"tag_name":"v1.1.0","html_url":"https://github.com/x/y/releases/tag/v1.1.0"})");
        QVERIFY(r.has_value());
        QCOMPARE(r->tag, QString("v1.1.0"));
        QCOMPARE(r->url, QString("https://github.com/x/y/releases/tag/v1.1.0"));
    }

    void refusesAnythingElse()
    {
        QVERIFY(!UpdateCheck::parseRelease("").has_value());
        QVERIFY(!UpdateCheck::parseRelease("not json").has_value());
        QVERIFY(!UpdateCheck::parseRelease(R"({"message":"Not Found"})").has_value());
    }
};

QTEST_APPLESS_MAIN(TestUpdateCheck)
#include "tst_updatecheck.moc"
