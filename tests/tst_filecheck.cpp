#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "Repository.h"

// Files from outside (backups, copies on a USB stick) are checked before they are used.
class TestFileCheck : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;

    // The tables exactly as the program makes them.
    const QStringList ourTables = {
        "CREATE TABLE weather (date TEXT PRIMARY KEY, high_c REAL, low_c REAL, wind_kmh REAL,"
        " wind_dir TEXT, precip_type TEXT, precip_mm REAL, humidity REAL, pressure_hpa REAL,"
        " cloud TEXT, gust_kmh REAL, gust_dir TEXT)",
        "CREATE TABLE notes (date TEXT PRIMARY KEY, text TEXT NOT NULL)",
        "CREATE TABLE events (date TEXT NOT NULL, key TEXT NOT NULL, PRIMARY KEY (date, key))",
        "CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT NOT NULL)",
    };

    // Makes a database file by running `statements` (any SQL, so it can be "tampered").
    QString make(const QString &name, const QStringList &statements)
    {
        const QString path = dir.filePath(name);
        QFile::remove(path);
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "craft");
            db.setDatabaseName(path);
            if (!db.open())
                qFatal("could not create a test file");
            for (const QString &sql : statements) {
                QSqlQuery q(db);
                if (!q.exec(sql))
                    qFatal("test SQL failed: %s", qPrintable(sql));
            }
            db.close();
        }
        QSqlDatabase::removeDatabase("craft");
        return path;
    }

    // Our tables plus `extra` statements.
    QString makeOurs(const QString &name, const QStringList &extra = {})
    {
        return make(name, ourTables + extra);
    }

    void expectRefused(const QString &path, const char *mustMention = nullptr)
    {
        const auto check = Repository::checkFile(path, true);
        QVERIFY(!check.ok);
        QVERIFY(!check.problem.isEmpty());
        if (mustMention)
            QVERIFY2(check.problem.contains(QString::fromUtf8(mustMention)), qPrintable(check.problem));
    }

private slots:
    void initTestCase() { QVERIFY(dir.isValid()); }

    void acceptsAFileTheProgramMade()
    {
        const QString path = dir.filePath("real.db");
        {
            Repository repo(path);
            WeatherDay d;
            d.highC = -41.5;
            d.lowC = -48;
            d.windKmh = 0;
            d.windDir = "NW";
            d.precipType = "Snow";
            d.precipMm = 2.5;
            d.humidityPct = 100;
            d.pressureHpa = 1042;
            d.cloud = "Overcast";
            d.gustKmh = 70;
            d.gustDir = "N";
            QVERIFY(repo.saveWeather(QDate(2026, 1, 15), d));
            QVERIFY(repo.saveNote(QDate(2026, 1, 15), "Cold! <b>not bold</b> & fine"));
            QVERIFY(repo.saveEvents(QDate(2026, 1, 15), {"first_snow"}));
            QVERIFY(repo.setSetting("units", "imperial"));
        }
        const auto check = Repository::checkFile(path, true);
        QVERIFY2(check.ok, qPrintable(check.problem));
        QCOMPARE(check.counts.weatherDays, 1);
        QCOMPARE(check.counts.noteDays, 1);
        QCOMPARE(check.counts.eventDays, 1);
        QVERIFY(Repository::inspect(path).has_value());
    }

    void acceptsOlderAndNewerFiles()
    {
        // Older: no gust columns, no settings table. Whole numbers stored as integers.
        const QString older = make("older.db", {
            "CREATE TABLE weather (date TEXT PRIMARY KEY, high_c REAL, low_c REAL, wind_kmh REAL,"
            " wind_dir TEXT, precip_type TEXT, precip_mm REAL, humidity REAL, pressure_hpa REAL,"
            " cloud TEXT)",
            "INSERT INTO weather (date, high_c, wind_kmh) VALUES ('2025-07-01', 31, 12)",
        });
        QVERIFY2(Repository::checkFile(older, true).ok, qPrintable(Repository::checkFile(older, true).problem));

        // Newer: an extra table and an extra column the program does not know yet.
        const QString newer = makeOurs("newer.db", {
            "ALTER TABLE weather ADD COLUMN snow_depth_mm REAL",
            "CREATE TABLE photos (date TEXT, file TEXT)",
        });
        QVERIFY(Repository::checkFile(newer, true).ok);
    }

    void refusesThingsThatAreNotOurData()
    {
        expectRefused(dir.filePath("missing.db"), "not Weather Tracker data");

        QFile empty(dir.filePath("empty.db"));
        QVERIFY(empty.open(QIODevice::WriteOnly));
        empty.close();
        expectRefused(empty.fileName(), "not Weather Tracker data");

        QFile text(dir.filePath("text.db"));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("this is not a database");
        text.close();
        expectRefused(text.fileName(), "not Weather Tracker data");

        expectRefused(make("other.db", {"CREATE TABLE recipes (name TEXT)"}),
                      "not Weather Tracker data");
    }

    void refusesAFileThatIsTooLarge()
    {
        const QString path = makeOurs("huge.db");
        QFile f(path);
        QVERIFY(f.resize(Repository::kMaxFileBytes + 1));   // quick: the extra space is empty
        expectRefused(path, "too large");
    }

    void refusesTablesWithTheWrongShape()
    {
        // No primary key: saving would add a second row for the same day.
        expectRefused(make("nokey.db", {
            "CREATE TABLE weather (date TEXT, high_c REAL, low_c REAL, wind_kmh REAL, wind_dir TEXT,"
            " precip_type TEXT, precip_mm REAL, humidity REAL, pressure_hpa REAL, cloud TEXT)",
        }), "not Weather Tracker data");

        // A missing column.
        expectRefused(make("nocolumn.db", {
            "CREATE TABLE weather (date TEXT PRIMARY KEY, high_c REAL)",
        }), "not Weather Tracker data");

        // Notes with the wrong key.
        expectRefused(make("badnotes.db", {
            ourTables[0],
            "CREATE TABLE notes (id INTEGER PRIMARY KEY, date TEXT, text TEXT)",
        }), "not Weather Tracker data");
    }

    void refusesCodeHiddenInTheFile()
    {
        expectRefused(makeOurs("trigger.db", {
            "CREATE TRIGGER wipe AFTER INSERT ON weather BEGIN DELETE FROM notes; END",
        }), "never makes");
        expectRefused(makeOurs("view.db", {"CREATE VIEW v AS SELECT * FROM weather"}),
                      "never makes");
    }

    void refusesValuesTheProgramCouldNotHaveSaved_data()
    {
        QTest::addColumn<QString>("sql");
        QTest::addColumn<QString>("mention");
        QTest::newRow("negative wind")
            << "INSERT INTO weather (date, wind_kmh) VALUES ('2021-03-03', -5)" << "March 3, 2021";
        QTest::newRow("negative gust")
            << "INSERT INTO weather (date, gust_kmh) VALUES ('2021-03-03', -1)" << "March 3, 2021";
        QTest::newRow("negative precipitation")
            << "INSERT INTO weather (date, precip_mm) VALUES ('2021-03-03', -0.5)" << "weather";
        QTest::newRow("humidity over 100")
            << "INSERT INTO weather (date, humidity) VALUES ('2021-03-03', 150)" << "weather";
        QTest::newRow("words for a number")
            << "INSERT INTO weather (date, high_c) VALUES ('2021-03-03', 'hot')" << "weather";
        QTest::newRow("infinity")
            << "INSERT INTO weather (date, low_c) VALUES ('2021-03-03', 1e999)" << "weather";
        QTest::newRow("absurd size")
            << "INSERT INTO weather (date, pressure_hpa) VALUES ('2021-03-03', 5e9)" << "weather";
        QTest::newRow("binary in a text column")
            << "INSERT INTO weather (date, cloud) VALUES ('2021-03-03', x'00ff')" << "weather";
        QTest::newRow("very long dropdown value")
            << "INSERT INTO weather (date, wind_dir) VALUES ('2021-03-03', printf('%.500c', 'N'))"
            << "weather";
        QTest::newRow("impossible date")
            << "INSERT INTO weather (date, high_c) VALUES ('2026-02-30', 1)" << "unreadable date";
        QTest::newRow("date written differently")
            << "INSERT INTO weather (date, high_c) VALUES ('2026-1-5', 1)" << "weather";
        QTest::newRow("number as a date")
            << "INSERT INTO weather (date, high_c) VALUES (20260105, 1)" << "weather";
        QTest::newRow("note that is not text")
            << "INSERT INTO notes (date, text) VALUES ('2021-03-03', x'0102')" << "note for March 3, 2021";
        QTest::newRow("enormous note")
            << "INSERT INTO notes (date, text) VALUES ('2021-03-03', printf('%.1000001c', 'a'))"
            << "note";
        QTest::newRow("event with a bad date")
            << "INSERT INTO events (date, key) VALUES ('yesterday', 'first_snow')" << "events";
        QTest::newRow("setting that is not text")
            << "INSERT INTO settings (key, value) VALUES ('units', x'00')" << "settings";
    }
    void refusesValuesTheProgramCouldNotHaveSaved()
    {
        QFETCH(QString, sql);
        QFETCH(QString, mention);
        const QString path = makeOurs("values.db", {sql});
        QVERIFY(Repository::checkFile(path, false).ok);     // the quick check (for lists) allows it
        expectRefused(path, qPrintable(mention));           // the thorough one does not
    }

    void refusesADamagedFile()
    {
        QStringList rows;
        for (int i = 0; i < 400; ++i)
            rows << QString("INSERT INTO notes (date, text) VALUES ('%1', '%2')")
                        .arg(QDate(2020, 1, 1).addDays(i).toString(Qt::ISODate),
                             QString(200, QChar('x')));
        const QString path = makeOurs("damaged.db", rows);

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.size() > 10 * 4096);
        f.seek(3 * 4096);                       // scribble over a few pages in the middle
        f.write(QByteArray(4 * 4096, '\xA5'));
        f.close();
        expectRefused(path);
    }

    void restoreRefusesABadFileAndKeepsTheData()
    {
        const QString current = dir.filePath("current.db");
        Repository repo(current);
        WeatherDay d;
        d.highC = 12;
        QVERIFY(repo.saveWeather(QDate(2026, 5, 1), d));

        const QString bad = makeOurs("bad.db", {
            "INSERT INTO weather (date, wind_kmh) VALUES ('2026-05-02', -3)",
        });
        QVERIFY(!repo.replaceWith(bad));
        QVERIFY(repo.lastError().contains("could not have saved"));
        QVERIFY(repo.isOpen());
        QCOMPARE(*repo.weatherFor(QDate(2026, 5, 1))->highC, 12.0);
        QVERIFY(!QFile::exists(current + ".restoring"));     // no leftovers
        QVERIFY(!QFile::exists(current + ".replaced"));
    }

    void restoreStillWorksForAGoodFile()
    {
        const QString current = dir.filePath("current2.db");
        Repository repo(current);
        const QString good = makeOurs("good.db", {
            "INSERT INTO weather (date, high_c) VALUES ('2026-05-03', 20.5)",
        });
        QVERIFY2(repo.replaceWith(good), qPrintable(repo.lastError()));
        QVERIFY(repo.isOpen());
        QCOMPARE(*repo.weatherFor(QDate(2026, 5, 3))->highC, 20.5);
        QVERIFY(!QFile::exists(current + ".restoring"));
        QVERIFY(!QFile::exists(current + ".replaced"));
    }
};

QTEST_GUILESS_MAIN(TestFileCheck)
#include "tst_filecheck.moc"
