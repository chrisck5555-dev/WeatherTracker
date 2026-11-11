#include "BannerPlan.h"

#include <QStringList>
#include <cmath>

namespace BannerPlan {

namespace {

// SplitMix64: small, fast and the same on every compiler and Qt version (std::mt19937's
// distributions are not), so a day's banner never changes after an update.
class Random
{
public:
    explicit Random(quint64 seed) : m_state(seed) {}

    quint64 next()
    {
        quint64 z = (m_state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    int below(int n) { return n <= 1 ? 0 : int((next() >> 11) % quint64(n)); }

private:
    quint64 m_state;
};

const QStringList kClouds = {"cloud-large-high", "cloud-large-low", "cloud-small-high",
                             "cloud-small-low"};
const QStringList kSunBehindCloud = {"sun_cloud-large-bottom-right", "sun_cloud-large-top-left",
                                     "sun_cloud-small-bottom-right", "sun_cloud-small-top-left"};
const QStringList kRain = {"rain", "cloud-large-high_rain", "cloud-small-high_rain",
                           "cloud-small-high-x2_rain"};
const QStringList kSnow = {"snow", "cloud-large-high_snow", "cloud-small-high_snow"};

// The squares that aren't the sun's. "clear" is listed more than once where most of the
// sky should be clear.
QStringList othersFor(Sky sky)
{
    switch (sky) {
    case Sky::Clear:        return {"clear"};
    case Sky::MostlyClear:  return {"clear", "clear", "clear", "cloud-small-high", "cloud-small-low"};
    case Sky::PartlyCloudy: return {"clear", "clear", "cloud-small-high", "cloud-small-low",
                                    "cloud-large-high", "cloud-large-low"};
    case Sky::MostlyCloudy:
    case Sky::Overcast:     return kClouds;
    case Sky::Rain:         return kRain;
    case Sky::Snow:         return kSnow;
    case Sky::Plain:        return {"clear"};
    }
    return {"clear"};
}

// The one square with the sun (or moon), if this sky has one.
QStringList bodyFor(Sky sky)
{
    switch (sky) {
    case Sky::Clear:
    case Sky::MostlyClear:
    case Sky::PartlyCloudy: return {"sun"};          // in the clear
    case Sky::MostlyCloudy: return kSunBehindCloud;  // behind a cloud
    default:                return {};
    }
}

} // namespace

Sky skyFor(const QString &precipType, const QString &cloud)
{
    if (precipType == "Snow")
        return Sky::Snow;
    if (precipType == "Rain" || precipType == "Drizzle")
        return Sky::Rain;
    if (!precipType.isEmpty() && precipType != "None")
        return Sky::Plain;                       // hail, sleet, freezing rain, mixed
    if (cloud == "Clear")         return Sky::Clear;
    if (cloud == "Mostly clear")  return Sky::MostlyClear;
    if (cloud == "Partly cloudy") return Sky::PartlyCloudy;
    if (cloud == "Mostly cloudy") return Sky::MostlyCloudy;
    if (cloud == "Overcast")      return Sky::Overcast;
    return Sky::Plain;
}

QString skyWords(Sky sky)
{
    switch (sky) {
    case Sky::Clear:        return QStringLiteral("clear");
    case Sky::MostlyClear:  return QStringLiteral("mostly clear");
    case Sky::PartlyCloudy: return QStringLiteral("partly cloudy");
    case Sky::MostlyCloudy: return QStringLiteral("mostly cloudy");
    case Sky::Overcast:     return QStringLiteral("overcast");
    case Sky::Rain:         return QStringLiteral("rain");
    case Sky::Snow:         return QStringLiteral("snow");
    case Sky::Plain:        return QStringLiteral("sky");
    }
    return QString();
}

quint64 seedFor(const QDate &date)
{
    return quint64(date.year()) * 10000 + quint64(date.month()) * 100 + quint64(date.day());
}

EdgeSlopes edgeSlopes(const Piece &p)
{
    if (p.h0 == p.h1)
        return p.concaveUp ? EdgeSlopes{-0.4, 0.4} : EdgeSlopes{0.4, -0.4};   // dip / bump
    if (p.h1 > p.h0)
        return p.concaveUp ? EdgeSlopes{0.1, 0.5} : EdgeSlopes{0.5, 0.1};     // rising
    return p.concaveUp ? EdgeSlopes{-0.5, -0.1} : EdgeSlopes{-0.1, -0.5};     // falling
}

QList<Square> plan(const QDate &date, Sky sky, int count)
{
    QList<Square> squares;
    Random r(seedFor(date));
    // Drawn first and from a fixed range, so it doesn't depend on how many squares fit.
    const int bodyAt = kBodyFirst + r.below(kBodySpan);
    const QStringList body = bodyFor(sky);
    const QStringList others = othersFor(sky);
    int height = r.below(3);
    double slope = 0;
    bool first = true, lastLevel = false;
    QString previous;
    for (int i = 0; i < count; ++i) {
        Square square;
        const QStringList &from = (i == bodyAt && !body.isEmpty()) ? body : others;
        square.sky = from[r.below(int(from.size()))];
        // Neighbours differ, except clear sky, which may repeat.
        for (int tries = 0; square.sky == previous && square.sky != "clear" && from.size() > 1
                            && tries < 8; ++tries)
            square.sky = from[r.below(int(from.size()))];
        previous = square.sky;

        // The hills: one smooth line all the way across. Each piece starts where the last
        // one ended, at the same height and nearly the same slope (kSmoothJoin), and rises or
        // falls by at most one step. A level dip or bump is never followed by another one
        // unless nothing else fits, so the line rolls instead of wobbling.
        for (Piece &piece : square.ground) {
            Piece options[6], level[6];
            int n = 0, nLevel = 0;
            for (int next = height - 1; next <= height + 1; ++next) {
                if (next < 0 || next > 2)
                    continue;
                for (bool up : {false, true}) {
                    const Piece candidate{height, next, up};
                    if (!first && std::abs(edgeSlopes(candidate).left - slope) > kSmoothJoin)
                        continue;
                    if (next == height && lastLevel)
                        level[nLevel++] = candidate;
                    else
                        options[n++] = candidate;
                }
            }
            piece = n > 0 ? options[r.below(n)] : level[r.below(nLevel)];
            lastLevel = piece.h0 == piece.h1;
            slope = edgeSlopes(piece).right;
            height = piece.h1;
            first = false;
        }
        squares.append(square);
    }
    return squares;
}

} // namespace BannerPlan
