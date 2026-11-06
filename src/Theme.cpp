#include "Theme.h"

#include <algorithm>
#include <cmath>

#include "Design.h"

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
            c.window = "#e9eef7";  c.surface = "#f6f8fd";  c.raised = "#f6f8fd";
            c.text = "#1f2a44";    c.mutedText = "#4a5878";
            c.border = "#c5cfe3";  c.highlight = "#ffffff";  c.shadow = "#b4c0da";
            c.inputBorder = "#7a87a5";
            c.accent = "#3f6fd8";  c.accentText = "#ffffff";
            c.error = "#b3261e";
            c.high = "#d6453d";    c.low = "#0f7c8c";
            c.rain = "#0f7c8c";    c.snow = "#6f8fb8";
            c.markWeather = "#3f6fd8"; c.markNote = "#c27400"; c.markEvent = "#23824a";
        } else {
            c.window = "#1d2233";  c.surface = "#2a3048";  c.raised = "#313852";
            c.text = "#eef1fa";    c.mutedText = "#b4bdd6";
            c.border = "#3d4663";  c.highlight = "#3c4566";  c.shadow = "#11141f";
            c.inputBorder = "#7b86ab";
            c.accent = "#5b8def";  c.accentText = "#0e1424";
            c.error = "#ff8a80";
            c.high = "#ff7b72";    c.low = "#5cc8d6";
            c.rain = "#5cc8d6";    c.snow = "#a9bfdc";
            c.markWeather = "#79a6ff"; c.markNote = "#f0b23c"; c.markEvent = "#5fd48a";
        }
    } else {
        if (!dark) {
            c.window = "#ffffff";  c.surface = "#ffffff";  c.raised = "#ffffff";
            c.text = "#000000";    c.mutedText = "#1a1a1a";
            c.border = "#000000";  c.highlight = "#ffffff";  c.shadow = "#000000";
            c.accent = "#0b3d91";  c.accentText = "#ffffff";
            c.error = "#a00000";
            c.high = "#b00000";    c.low = "#005f6b";
            c.rain = "#005f6b";    c.snow = "#4d4d4d";
            c.markWeather = "#0b3d91"; c.markNote = "#8a4b00"; c.markEvent = "#006400";
        } else {
            c.window = "#000000";  c.surface = "#000000";  c.raised = "#000000";
            c.text = "#ffffff";    c.mutedText = "#f0f0f0";
            c.border = "#ffffff";  c.highlight = "#000000";  c.shadow = "#000000";
            c.accent = "#ffd400";  c.accentText = "#000000";
            c.error = "#ff6b6b";
            c.high = "#ff6b6b";    c.low = "#6bb7ff";
            c.rain = "#6bb7ff";    c.snow = "#d0d0d0";
            c.markWeather = "#6bb7ff"; c.markNote = "#ffd400"; c.markEvent = "#5dff8f";
        }
    }
    if (style == Style::HighContrast) {
        c.inputBorder = c.border;                   // already black or white
        // Muted text is nearly the text colour here, so disabled controls get a grey that
        // still meets 4.5:1 (ST5) but can be told apart from the enabled ones.
        c.disabledText = dark ? QColor("#a6a6a6") : QColor("#6b6b6b");
    } else {
        c.disabledText = c.mutedText;
    }
    return c;
}

QColor mix(const QColor &base, const QColor &top, double amount)
{
    const double a = std::clamp(amount, 0.0, 1.0);
    return QColor::fromRgbF(float(base.redF() + (top.redF() - base.redF()) * a),
                            float(base.greenF() + (top.greenF() - base.greenF()) * a),
                            float(base.blueF() + (top.blueF() - base.blueF()) * a));
}

namespace {
QColor withAlpha(QColor c, double alpha)
{
    c.setAlphaF(float(alpha));
    return c;
}
}

double lightness(const QColor &c)
{
    const double y = luminance(c);
    const double f = y > 216.0 / 24389.0 ? std::cbrt(y) : (24389.0 / 27.0 * y + 16.0) / 116.0;
    return 116.0 * f - 16.0;
}

namespace {
// The tint toward the text colour that changes `fill`'s lightness as much as `amount`
// changes the surface's. Never weaker than `amount`, and at most 75% so the fill keeps
// some of its own colour.
double matchedAmount(const Colors &c, const QColor &fill, double amount)
{
    if (!fill.isValid() || fill.alpha() == 0)
        return amount;
    const double target = std::abs(lightness(mix(c.surface, c.text, amount)) - lightness(c.surface));
    const double base = lightness(fill);
    double lo = 0.0, hi = 0.75;
    for (int i = 0; i < 20; ++i) {
        const double mid = (lo + hi) / 2.0;
        if (std::abs(lightness(mix(fill, c.text, mid)) - base) < target)
            lo = mid;
        else
            hi = mid;
    }
    return std::max(amount, hi);
}
}

QColor hoverOverlay(const Colors &c, const QColor &fill)
{
    return withAlpha(c.text, matchedAmount(c, fill, Design::kHoverTint));
}

QColor pressOverlay(const Colors &c, const QColor &fill)
{
    return withAlpha(c.text, matchedAmount(c, fill, Design::kPressTint));
}

QColor focusOverlay(Style style, const Colors &c, bool onAccent)
{
    const double amount = style == Style::HighContrast ? Design::kFocusTintContrast : Design::kFocusTint;
    if (!onAccent)
        return withAlpha(c.accent, amount);

    // On the accent: try both candidates and keep the one that leaves the label readable
    // (C1: 4.5:1, 7:1 in high contrast) while changing the fill the most.
    const double minimum = style == Style::HighContrast ? 7.0 : 4.5;
    QColor best = c.text;
    double bestChange = -1;
    for (const QColor &candidate : {c.text, c.accentText}) {
        const QColor result = mix(c.accent, candidate, amount);
        if (contrastRatio(c.accentText, result) < minimum)
            continue;
        const double change = contrastRatio(result, c.accent);
        if (change > bestChange) {
            bestChange = change;
            best = candidate;
        }
    }
    return withAlpha(best, amount);
}

QColor smoke() { return withAlpha(QColor(0, 0, 0), Design::kSmoke); }

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