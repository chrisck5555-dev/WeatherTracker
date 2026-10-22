#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QRandomGenerator>
#include <QTextStream>
#include <cmath>

#include "Repository.h"
#include "WeatherInput.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QString path = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString("demo.db");
    QFile::remove(path);                       // always start fresh
    Repository repo(path);
    if (!repo.isOpen()) {
        out << "Could not create " << path << ": " << repo.lastError() << "\n";
        return 1;
    }

    QRandomGenerator rng(12345);               // fixed seed: the same data every run
    auto uniform = [&](double lo, double hi) { return lo + (hi - lo) * rng.generateDouble(); };

    const QDate end = QDate::currentDate().addDays(-1);
    const QDate start = end.addYears(-5);
    const QStringList clouds = WeatherInput::cloudCovers();
    int written = 0;

    for (QDate d = start; d <= end; d = d.addDays(1)) {
        if (rng.bounded(10) == 0)
            continue;                          // about 10% of days have nothing

        // Seasonal curve: coldest in mid-January (day 15), warmest in mid-July.
        const double season = std::cos((d.dayOfYear() - 15) * 2.0 * M_PI / 365.25);
        const double mean = 6.0 - 14.0 * season;           // about -8 to +20 C
        WeatherDay w;
        w.highC = std::round((mean + 5 + uniform(-4, 4)) * 10) / 10;
        w.lowC = std::round((mean - 4 + uniform(-3, 3)) * 10) / 10;
        w.windKmh = std::round(uniform(2, 35) * 10) / 10;
        w.humidityPct = std::round(uniform(35, 95));
        w.pressureHpa = std::round((1013 + uniform(-18, 18)) * 10) / 10;
        w.cloud = clouds[rng.bounded(int(clouds.size()))];

        // Prevailing wind: mostly W in winter, mostly SW in summer, sometimes anything
        const QStringList dirs = WeatherInput::windDirections();
        if (rng.bounded(100) < 65)
            w.windDir = season > 0 ? "W" : "SW";
        else
            w.windDir = dirs[rng.bounded(int(dirs.size()))];

        // Precipitation depends on the temperature
        if (rng.bounded(100) < 40) {
            if (*w.highC < 1.0) {
                w.precipType = "Snow";
                w.precipMm = std::round(uniform(10, 150));       // depth in mm (1-15 cm)
            } else {
                w.precipType = rng.bounded(3) == 0 ? "Drizzle" : "Rain";
                w.precipMm = std::round(uniform(0.5, 20) * 10) / 10;
            }
        } else {
            w.precipType = "None";
            w.precipMm = 0.0;
        }
        repo.saveWeather(d, w);
        ++written;

        if (rng.bounded(40) == 0)
            repo.saveNote(d, "Demo note for " + d.toString("MMMM d"));
        if (d.month() == 11 && d.day() == 5)
            repo.saveEvents(d, QSet<QString>{"first_snow"});
    }
    out << "Wrote " << written << " days to " << path << "\n";
    return 0;
}