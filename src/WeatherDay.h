#pragma once

#include <QString>
#include <optional>
#include <tuple>

// One day of recorded weather, always in metric (Requirements §5).
// An unset std::optional / empty QString means "Not recorded".
struct WeatherDay {
    std::optional<double> highC, lowC, windKmh, precipMm, humidityPct, pressureHpa;
    std::optional<double> gustKmh;
    QString windDir;      // "N", "NE", ... "NW"
    QString gustDir;      // same choices as windDir
    QString precipType;   // "None", "Rain", "Drizzle", ...
    QString cloud;        // "Clear", ... "Overcast"

    bool isEmpty() const
    {
        return !highC && !lowC && !windKmh && !precipMm && !humidityPct && !pressureHpa
            && !gustKmh && windDir.isEmpty() && gustDir.isEmpty()
            && precipType.isEmpty() && cloud.isEmpty();
    }

    bool operator==(const WeatherDay &o) const
    {
        return std::tie(highC, lowC, windKmh, precipMm, humidityPct, pressureHpa,
                        windDir, precipType, cloud, gustKmh, gustDir)
            == std::tie(o.highC, o.lowC, o.windKmh, o.precipMm, o.humidityPct,
                        o.pressureHpa, o.windDir, o.precipType, o.cloud,
                        o.gustKmh, o.gustDir);
    }
};