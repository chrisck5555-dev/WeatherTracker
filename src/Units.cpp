#include "Units.h"

namespace Units {

namespace {
constexpr double kKmPerMile = 1.609344;
constexpr double kMmPerInch = 25.4;
constexpr double kHpaPerInHg = 33.8638866667;
bool imp(System s) { return s == System::Imperial; }
} // namespace

System fromSetting(const QString &text)
{
    return text == "imperial" ? System::Imperial : System::Metric;
}
QString toSetting(System s) { return imp(s) ? "imperial" : "metric"; }

double cToDisplay(double c, System s)       { return imp(s) ? c * 9.0 / 5.0 + 32.0 : c; }
double displayToC(double f, System s)       { return imp(s) ? (f - 32.0) * 5.0 / 9.0 : f; }
double deltaCToDisplay(double d, System s)  { return imp(s) ? d * 9.0 / 5.0 : d; }

double kmhToDisplay(double k, System s)     { return imp(s) ? k / kKmPerMile : k; }
double displayToKmh(double m, System s)     { return imp(s) ? m * kKmPerMile : m; }

double hpaToDisplay(double h, System s)     { return imp(s) ? h / kHpaPerInHg : h; }
double displayToHpa(double i, System s)     { return imp(s) ? i * kHpaPerInHg : i; }

double mmToDisplay(double mm, System s)     { return imp(s) ? mm / kMmPerInch : mm; }
double displayToMm(double v, System s)      { return imp(s) ? v * kMmPerInch : v; }

double snowMmToDisplay(double mm, System s) { return imp(s) ? mm / kMmPerInch : mm / 10.0; }
double displayToSnowMm(double v, System s)  { return imp(s) ? v * kMmPerInch : v * 10.0; }

QString tempUnit(System s)     { return imp(s) ? "\u00B0F" : "\u00B0C"; }
QString windUnit(System s)     { return imp(s) ? "mph" : "km/h"; }
QString pressureUnit(System s) { return imp(s) ? "inHg" : "hPa"; }
QString rainUnit(System s)     { return imp(s) ? "in" : "mm"; }
QString snowUnit(System s)     { return imp(s) ? "in" : "cm"; }

} // namespace Units