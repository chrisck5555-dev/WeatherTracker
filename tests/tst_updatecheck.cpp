#include <QtTest>

#include "UpdateCheck.h"

// Plain strings (not R"(...)") so older Qt tools read this file too.
class TestUpdateCheck : public QObject
{
    Q_OBJECT

private:
    const QString api = "https://api.github.com/repos/chrisck5555-dev/WeatherTracker/releases/latest";

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
            "{\"tag_name\":\"v1.1.0\",\"html_url\":\"https://github.com/x/y/releases/tag/v1.1.0\"}");
        QVERIFY(r.has_value());
        QCOMPARE(r->tag, QString("v1.1.0"));
        QCOMPARE(r->url, QString("https://github.com/x/y/releases/tag/v1.1.0"));
    }

    void refusesAnythingElse()
    {
        QVERIFY(!UpdateCheck::parseRelease("").has_value());
        QVERIFY(!UpdateCheck::parseRelease("not json").has_value());
        QVERIFY(!UpdateCheck::parseRelease("{\"message\":\"Not Found\"}").has_value());
    }

    void refusesATagThatIsNotAVersion()
    {
        QVERIFY(!UpdateCheck::parseRelease("{\"tag_name\":\"<b>9.9</b>\"}").has_value());
        QVERIFY(!UpdateCheck::parseRelease("{\"tag_name\":\"latest\"}").has_value());
        QVERIFY(!UpdateCheck::parseRelease("{\"tag_name\":\"v1.0.0 please click here\"}").has_value());
    }

    void versionTags_data()
    {
        QTest::addColumn<QString>("tag");
        QTest::addColumn<bool>("ok");
        QTest::newRow("v1.1.0")        << "v1.1.0" << true;
        QTest::newRow("1.1.0")         << "1.1.0" << true;
        QTest::newRow("1.2")           << "1.2" << true;
        QTest::newRow("beta")          << "v1.2.0-beta" << true;
        QTest::newRow("empty")         << "" << false;
        QTest::newRow("words")         << "newest" << false;
        QTest::newRow("html")          << "v1.0<script>" << false;
        QTest::newRow("space")         << "v1. 0" << false;
        QTest::newRow("new line")      << "v1.0\nInstall now" << false;
        QTest::newRow("huge number")   << "v123456.0" << false;
        QTest::newRow("long suffix")   << "v1.0-" + QString(40, 'a') << false;
    }
    void versionTags()
    {
        QFETCH(QString, tag);
        QFETCH(bool, ok);
        QCOMPARE(UpdateCheck::isVersionTag(tag), ok);
    }

    void releasePagesComeFromTheApiAddress()
    {
        QCOMPARE(UpdateCheck::releasePagesFor(api),
                 QString("https://github.com/chrisck5555-dev/WeatherTracker/releases/"));
        QVERIFY(UpdateCheck::isSafeApiUrl(api));

        // Not HTTPS, another site, or another form: refused.
        QVERIFY(!UpdateCheck::isSafeApiUrl(
            "http://api.github.com/repos/chrisck5555-dev/WeatherTracker/releases/latest"));
        QVERIFY(!UpdateCheck::isSafeApiUrl(
            "https://api.github.com.example.com/repos/a/b/releases/latest"));
        QVERIFY(!UpdateCheck::isSafeApiUrl("https://user@api.github.com/repos/a/b/releases/latest"));
        QVERIFY(!UpdateCheck::isSafeApiUrl("https://api.github.com/repos/a/b/releases"));
        QVERIFY(!UpdateCheck::isSafeApiUrl("https://api.github.com/repos/../b/releases/latest"));
        QVERIFY(!UpdateCheck::isSafeApiUrl("https://api.github.com/repos/a/b/releases/latest?x=1"));
        QVERIFY(!UpdateCheck::isSafeApiUrl(""));
    }

    void opensOnlyThisRepositorysReleasePages_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("ok");
        QTest::newRow("release page")
            << "https://github.com/chrisck5555-dev/WeatherTracker/releases/tag/v1.1.0" << true;
        QTest::newRow("latest page")
            << "https://github.com/chrisck5555-dev/WeatherTracker/releases/latest" << true;
        QTest::newRow("other letter case")
            << "https://github.com/ChrisCK5555-dev/weathertracker/releases/tag/v1.1.0" << true;
        QTest::newRow("plain http")
            << "http://github.com/chrisck5555-dev/WeatherTracker/releases/tag/v1.1.0" << false;
        QTest::newRow("look-alike site")
            << "https://github.com.example.com/chrisck5555-dev/WeatherTracker/releases/" << false;
        QTest::newRow("name before @")
            << "https://github.com@example.com/chrisck5555-dev/WeatherTracker/releases/" << false;
        QTest::newRow("other repository")
            << "https://github.com/someone-else/WeatherTracker/releases/tag/v9.0.0" << false;
        QTest::newRow("climbing out")
            << "https://github.com/chrisck5555-dev/WeatherTracker/releases/../../../x" << false;
        QTest::newRow("encoded climbing out")
            << "https://github.com/chrisck5555-dev/WeatherTracker/releases/%2e%2e/%2e%2e/x" << false;
        QTest::newRow("other port")
            << "https://github.com:8443/chrisck5555-dev/WeatherTracker/releases/" << false;
        QTest::newRow("a file")  << "file:///C:/Windows/System32/calc.exe" << false;
        QTest::newRow("script")  << "javascript:alert(1)" << false;
        QTest::newRow("empty")   << "" << false;
    }
    void opensOnlyThisRepositorysReleasePages()
    {
        QFETCH(QString, url);
        QFETCH(bool, ok);
        QCOMPARE(UpdateCheck::isSafeReleaseUrl(url, api), ok);
    }
};

QTEST_APPLESS_MAIN(TestUpdateCheck)
#include "tst_updatecheck.moc"
