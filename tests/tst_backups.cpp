#include <QtTest>

#include "Backups.h"
#include "Repository.h"

class TestBackups : public QObject
{
    Q_OBJECT

private slots:
    void backupHoldsTheData()
    {
        QTemporaryDir dir;
        Repository repo(dir.filePath("weather.db"));
        WeatherDay d;
        d.highC = 21.5;
        repo.saveWeather(QDate(2026, 7, 1), d);
        repo.saveNote(QDate(2026, 7, 1), "hello");

        QVERIFY(Backups::backupNow(repo, nullptr, dir.filePath("b"), QDate(2026, 7, 2)));
        const auto list = Backups::list(dir.filePath("b"));
        QCOMPARE(list.size(), 1);
        QCOMPARE(list[0].counts.weatherDays, 1);
        QCOMPARE(list[0].counts.noteDays, 1);
        QCOMPARE(list[0].day, QDate(2026, 7, 2));
    }

    void sameDayReplacesAndOldOnesArePruned()
    {
        QTemporaryDir dir;
        Repository repo(dir.filePath("weather.db"));
        WeatherDay d;
        d.highC = 1;
        repo.saveWeather(QDate(2026, 7, 1), d);
        const QString folder = dir.filePath("b");

        for (int i = 0; i < Backups::kKeep + 3; ++i)
            QVERIFY(Backups::backupNow(repo, nullptr, folder, QDate(2026, 7, 1).addDays(i)));
        QCOMPARE(Backups::list(folder).size(), Backups::kKeep);

        // The oldest days are the ones that went
        QVERIFY(!Backups::existsFor(QDate(2026, 7, 1), folder));
        QVERIFY(Backups::existsFor(QDate(2026, 7, 1).addDays(Backups::kKeep + 2), folder));

        // A second backup on the same day replaces the first one
        repo.saveWeather(QDate(2026, 7, 2), d);
        const QDate last = QDate(2026, 7, 1).addDays(Backups::kKeep + 2);
        QVERIFY(Backups::backupNow(repo, nullptr, folder, last));
        QCOMPARE(Backups::list(folder).size(), Backups::kKeep);
        QCOMPARE(Backups::list(folder).first().counts.weatherDays, 2);
    }

    void restoreReplacesTheData()
    {
        QTemporaryDir dir;
        Repository repo(dir.filePath("weather.db"));
        WeatherDay d;
        d.highC = 10;
        repo.saveWeather(QDate(2026, 7, 1), d);
        const QString folder = dir.filePath("b");
        QVERIFY(Backups::backupNow(repo, nullptr, folder, QDate(2026, 7, 5)));

        d.highC = 30;
        repo.saveWeather(QDate(2026, 7, 2), d);                   // changes after the backup
        QCOMPARE(repo.counts().weatherDays, 2);

        QVERIFY(repo.replaceWith(Backups::pathFor(QDate(2026, 7, 5), folder)));
        QVERIFY(repo.isOpen());
        QCOMPARE(repo.counts().weatherDays, 1);
        QCOMPARE(*repo.weatherFor(QDate(2026, 7, 1))->highC, 10.0);
    }

    void refusesAFileThatIsNotOurs()
    {
        QTemporaryDir dir;
        Repository repo(dir.filePath("weather.db"));
        QFile junk(dir.filePath("junk.db"));
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write("this is not a database");
        junk.close();
        QVERIFY(!Repository::inspect(dir.filePath("junk.db")).has_value());
        QVERIFY(!repo.replaceWith(dir.filePath("junk.db")));
        QVERIFY(repo.isOpen());                                   // still working afterwards
    }
};

QTEST_GUILESS_MAIN(TestBackups)
#include "tst_backups.moc"
