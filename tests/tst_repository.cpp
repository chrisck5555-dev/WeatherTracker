#include <QtTest> 
#include <memory>

#include "Repository.h"
#include "ProblemReport.h"

class TestRepository : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Repository> repo;
    const QDate d{2026, 10, 5};

private slots:
    void init()      // runs before EACH test: a fresh, empty in-memory database
    {
        repo = std::make_unique<Repository>(":memory:");
        QVERIFY(repo->isOpen());
    }
    void cleanup() { repo.reset(); }

    void emptyDatabase()
    {
        QVERIFY(!repo->weatherFor(d).has_value());
        QCOMPARE(repo->note(d), QString());
        QVERIFY(repo->events(d).isEmpty());
    }

    void weatherRoundTrip()
    {
        WeatherDay w;
        w.highC = 21.3;
        w.lowC = -4.7;
        w.windDir = "NW";
        w.precipType = "Rain";
        w.precipMm = 0.0;                        // recorded as zero...
        QVERIFY(repo->saveWeather(d, w));

        const auto back = repo->weatherFor(d);
        QVERIFY(back.has_value());
        QVERIFY(*back == w);
        QVERIFY(*back->highC == 21.3);           // exact == on purpose: no drift allowed
        QVERIFY(back->precipMm.has_value());     // ...so it stays 0, not "missing"
        QVERIFY(!back->windKmh.has_value());     // never set: still "not recorded"
    }

    void savingEmptyDayDeletesRow()
    {
        WeatherDay w;
        w.highC = 10.0;
        QVERIFY(repo->saveWeather(d, w));
        QVERIFY(repo->weatherFor(d).has_value());
        QVERIFY(repo->saveWeather(d, WeatherDay{}));
        QVERIFY(!repo->weatherFor(d).has_value());
    }

    void rangeIsInclusive()
    {
        for (int day = 1; day <= 5; ++day) {
            WeatherDay w;
            w.highC = day;
            QVERIFY(repo->saveWeather(QDate(2026, 10, day), w));
        }
        const auto range = repo->weatherInRange(QDate(2026, 10, 2), QDate(2026, 10, 4));
        QCOMPARE(int(range.size()), 3);
        QCOMPARE(range.firstKey(), QDate(2026, 10, 2));
        QCOMPARE(range.lastKey(), QDate(2026, 10, 4));
    }

    void notes()
    {
        QVERIFY(repo->saveNote(d, "Windy afternoon"));
        QCOMPARE(repo->note(d), QString("Windy afternoon"));
        QVERIFY(repo->saveNote(d, "   "));        // blank = delete
        QCOMPARE(repo->note(d), QString());
    }

    void quotesAreSafe()
    {
        const QString nasty = "O'Brien'); DROP TABLE notes;--";
        QVERIFY(repo->saveNote(d, nasty));
        QCOMPARE(repo->note(d), nasty);
    }

    void events()
    {
        QVERIFY(repo->saveEvents(d, QSet<QString>{"first_snow", "first_frost"}));
        QCOMPARE(int(repo->events(d).size()), 2);
        QVERIFY(repo->saveEvents(d, QSet<QString>{"first_frost"}));   // replaces
        QCOMPARE(int(repo->events(d).size()), 1);
        QVERIFY(repo->events(d).contains("first_frost"));
        QVERIFY(repo->saveEvents(d, QSet<QString>{}));
        QVERIFY(repo->events(d).isEmpty());
    }

    void markerDates()
    {
        QVERIFY(repo->saveNote(QDate(2026, 10, 5), "x"));
        QVERIFY(repo->saveEvents(QDate(2026, 10, 6), QSet<QString>{"all_melted"}));
        const QDate a(2026, 10, 1), b(2026, 10, 31);
        QCOMPARE(repo->datesWithNotes(a, b), QSet<QDate>{QDate(2026, 10, 5)});
        QCOMPARE(repo->datesWithEvents(a, b), QSet<QDate>{QDate(2026, 10, 6)});
        QVERIFY(repo->datesWithNotes(QDate(2026, 11, 1), QDate(2026, 11, 30)).isEmpty());
    }

    void settings()
    {
        QCOMPARE(repo->setting("units", "metric"), QString("metric"));   // default
        QVERIFY(repo->setSetting("units", "imperial"));
        QCOMPARE(repo->setting("units", "metric"), QString("imperial"));
        QVERIFY(repo->setSetting("units", "metric"));                    // overwrite
        QCOMPARE(repo->setting("units"), QString("metric"));
    }

    void firstWeatherDate()
    {
        QVERIFY(!repo->firstWeatherDate().has_value());
        WeatherDay w;
        w.highC = 1.0;
        QVERIFY(repo->saveWeather(QDate(2025, 3, 1), w));
        QVERIFY(repo->saveWeather(QDate(2024, 7, 9), w));
        QCOMPARE(*repo->firstWeatherDate(), QDate(2024, 7, 9));
    }

    void countsAndReportContainNoData()
    {
        WeatherDay w;
        w.highC = 12.3456;
        QVERIFY(repo->saveWeather(d, w));
        QVERIFY(repo->saveNote(d, "SECRET-NOTE-TEXT"));
        QVERIFY(repo->saveEvents(d, QSet<QString>{"first_snow"}));
        const auto c = repo->counts();
        QCOMPARE(c.weatherDays, 1);
        QCOMPARE(c.noteDays, 1);
        QCOMPARE(c.eventDays, 1);
        const QString report = ProblemReport::build(*repo, QString());
        QVERIFY(report.contains("Days with weather: 1"));
        QVERIFY(!report.contains("SECRET-NOTE-TEXT"));
        QVERIFY(!report.contains("12.3456"));
    }

    void reportHidesTheUserName()
    {
        const QString log =
            "2026-10-27 10:00:00  v1.0.0: Could not open C:\\Users\\Chris\\AppData\\Roaming\\WeatherTracker\\weather.db\n"
            "2026-10-27 10:00:01  v1.0.0: Backup to C:/Users/Chris/Documents/Weather Tracker backups failed\n"
            "2026-10-27 10:00:02  v1.0.0: Signed in as chris; Christmas notes untouched";
        const QString out = ProblemReport::redact(log, "C:/Users/Chris", "Chris");
        QVERIFY2(!out.contains("Users\\Chris") && !out.contains("Users/Chris"), qPrintable(out));
        QVERIFY(out.contains("<user>\\AppData\\Roaming"));
        QVERIFY(out.contains("<user>/Documents"));
        QVERIFY(out.contains("Signed in as <user>;"));           // any letter case
        QVERIFY(out.contains("Christmas"));                        // part of another word: kept
        // Very short names are not replaced (they would mangle ordinary words).
        QCOMPARE(ProblemReport::redact("Al was here", "C:/Users/Al", "Al"), QString("Al was here"));
    }

    void deleteDayRemovesOnlyThatDay()
    {
        WeatherDay w;
        w.highC = 5;
        const QDate other = d.addDays(1);
        for (const QDate &day : {d, other}) {
            QVERIFY(repo->saveWeather(day, w));
            QVERIFY(repo->saveNote(day, "note"));
            QVERIFY(repo->saveEvents(day, QSet<QString>{"first_frost"}));
        }
        QVERIFY(repo->deleteDay(d));
        QVERIFY(!repo->weatherFor(d).has_value());
        QCOMPARE(repo->note(d), QString());
        QVERIFY(repo->events(d).isEmpty());
        QVERIFY(repo->weatherFor(other).has_value());              // the next day is untouched
        QCOMPARE(repo->note(other), QString("note"));
        QVERIFY(!repo->events(other).isEmpty());
    }

    void eraseAllKeepsSettings()
    {
        WeatherDay w;
        w.highC = 5;
        QVERIFY(repo->saveWeather(d, w));
        QVERIFY(repo->saveNote(d, "note"));
        QVERIFY(repo->setSetting("units", "imperial"));
        QVERIFY(repo->eraseAll());
        const auto c = repo->counts();
        QCOMPARE(c.weatherDays + c.noteDays + c.eventDays, 0);
        QCOMPARE(repo->setting("units"), QString("imperial"));
    }

    void erasedNotesAreGoneFromTheFile()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("weather.db");
        {
            Repository real(file);
            QVERIFY(real.saveNote(d, "MY-PRIVATE-NOTE-0123456789"));
            QVERIFY(real.saveNote(d.addDays(1), "ANOTHER-PRIVATE-NOTE-9876543210"));
            QVERIFY(real.deleteDay(d));                             // one day, then everything
            QVERIFY(real.eraseAll());
        }
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray bytes = f.readAll();
        QVERIFY(!bytes.contains("MY-PRIVATE-NOTE"));
        QVERIFY(!bytes.contains("ANOTHER-PRIVATE-NOTE"));
    }

    void deletedDayIsWipedFromTheFileToo()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("weather.db");
        {
            Repository real(file);
            QVERIFY(real.saveNote(d, "WIPE-ME-PLEASE-0123456789"));
            QVERIFY(real.saveNote(d.addDays(1), "keep me"));
            QVERIFY(real.deleteDay(d));
        }
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!f.readAll().contains("WIPE-ME-PLEASE"));            // secure_delete is on
    }
};

QTEST_GUILESS_MAIN(TestRepository)
#include "tst_repository.moc"