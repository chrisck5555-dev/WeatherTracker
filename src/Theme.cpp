#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace Theme {

namespace {
double channel(double c)
{
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
double luminance(const QColor &c)
{
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}
}

double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

Colors colors(Style style, bool dark)
{
    Colors c;
    if (style == Style::Clay) {
        if (!dark) {
            c.window = "#e9eef7";  c.surface = "#f6f8fd";
            c.text = "#1f2a44";    c.mutedText = "#4a5878";
            c.border = "#c5cfe3";  c.highlight = "#ffffff";  c.shadow = "#b4c0da";
            c.accent = "#3f6fd8";  c.accentText = "#ffffff";
            c.error = "#b3261e";
            c.high = "#d6453d";    c.low = "#3f6fd8";
            c.rain = "#3f6fd8";    c.snow = "#6f8fb8";
            c.markWeather = "#3f6fd8"; c.markNote = "#c27400"; c.markEvent = "#23824a";
        } else {
            c.window = "#1d2233";  c.surface = "#2a3048";
            c.text = "#eef1fa";    c.mutedText = "#b4bdd6";
            c.border = "#3d4663";  c.highlight = "#3c4566";  c.shadow = "#11141f";
            c.accent = "#5b8def";  c.accentText = "#0e1424";
            c.error = "#ff8a80";
            c.high = "#ff7b72";    c.low = "#79a6ff";
            c.rain = "#79a6ff";    c.snow = "#a9bfdc";
            c.markWeather = "#79a6ff"; c.markNote = "#f0b23c"; c.markEvent = "#5fd48a";
        }
    } else {
        if (!dark) {
            c.window = "#ffffff";  c.surface = "#ffffff";
            c.text = "#000000";    c.mutedText = "#1a1a1a";
            c.border = "#000000";  c.highlight = "#ffffff";  c.shadow = "#000000";
            c.accent = "#0b3d91";  c.accentText = "#ffffff";
            c.error = "#a00000";
            c.high = "#b00000";    c.low = "#0b3d91";
            c.rain = "#0b3d91";    c.snow = "#4d4d4d";
            c.markWeather = "#0b3d91"; c.markNote = "#8a4b00"; c.markEvent = "#006400";
        } else {
            c.window = "#000000";  c.surface = "#000000";
            c.text = "#ffffff";    c.mutedText = "#f0f0f0";
            c.border = "#ffffff";  c.highlight = "#000000";  c.shadow = "#000000";
            c.accent = "#ffd400";  c.accentText = "#000000";
            c.error = "#ff6b6b";
            c.high = "#ff6b6b";    c.low = "#6bb7ff";
            c.rain = "#6bb7ff";    c.snow = "#d0d0d0";
            c.markWeather = "#6bb7ff"; c.markNote = "#ffd400"; c.markEvent = "#5dff8f";
        }
    }
    return c;
}

QString styleSetting(Style s) { return s == Style::HighContrast ? "contrast" : "clay"; }
Style styleFromSetting(const QString &t) { return t == "contrast" ? Style::HighContrast : Style::Clay; }

QString brightnessSetting(Brightness b)
{
    switch (b) {
    case Brightness::Light:  return "light";
    case Brightness::Dark:   return "dark";
    case Brightness::System: return "system";
    }
    return "system";
}
Brightness brightnessFromSetting(const QString &t)
{
    if (t == "light") return Brightness::Light;
    if (t == "dark")  return Brightness::Dark;
    return Brightness::System;
}

} // namespace Theme