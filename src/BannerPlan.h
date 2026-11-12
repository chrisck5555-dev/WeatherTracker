#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <array>

// The Day view's banner (IL1): a row of squares, each a sky with three ground pieces along
// its bottom (resources/banner-squares/README.md). Which squares, and the hills' heights, are
// chosen at random from a seed made from the date: every day looks different, and the same
// day always looks the same, at any window size.
namespace BannerPlan {

// What the banner shows, from what was recorded (IL1).
enum class Sky {
    Clear,          // one sun, clear sky
    MostlyClear,    // one sun, mostly clear sky, a few small clouds
    PartlyCloudy,   // one sun in the clear, some clouds, some sky
    MostlyCloudy,   // clouds, and the sun behind a cloud in one square
    Overcast,       // clouds in every square
    Rain,
    Snow,
    Plain,          // clear sky only: hail, sleet, freezing rain, mixed, or nothing recorded
};

// Precipitation first, then cloud cover (IL1).
Sky skyFor(const QString &precipType, const QString &cloud);
QString skyWords(Sky sky);                 // "partly cloudy", for Narrator ("Picture: ...")

// One ground piece: heights at its left and right edges (0 low, 1 mid, 2 high; they differ
// by at most 1) and whether it dips (concave up) or bulges (concave down).
struct Piece {
    int h0 = 0, h1 = 0;
    bool concaveUp = false;
};

// The ground's slope where a piece meets its neighbours (rise per px, as the pieces are
// drawn; measured from the art): a rising or falling piece is steep (0.5) at one end and
// nearly level (0.1) at the other; a level piece's dip or bump leaves and arrives at 0.4.
struct EdgeSlopes {
    double left, right;
};
EdgeSlopes edgeSlopes(const Piece &piece);
// Two pieces join smoothly when the slope changes by at most this much. It allows a hill's
// rounded top and a valley's bottom (0.1 up to 0.1 down) but no corners.
inline constexpr double kSmoothJoin = 0.21;

struct Square {
    QString sky;                           // the sky file's parts: "sun", "clear",
                                           // "cloud-small-high", "sun_cloud-large-top-left" ...
                                           // ("sun" is drawn as the moon at night)
    std::array<Piece, 3> ground;           // left, middle, right
};

// The sun (or moon) is always in the third, fourth or fifth square: in view in the smallest
// window, and in the same place at every window size.
inline constexpr int kBodyFirst = 2;
inline constexpr int kBodySpan = 3;

quint64 seedFor(const QDate &date);        // from the year, month and day

// The first `count` squares for this date. A longer list starts with the same squares, so a
// wider window only adds squares at the right.
QList<Square> plan(const QDate &date, Sky sky, int count);

} // namespace BannerPlan
