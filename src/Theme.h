#pragma once

#include <QColor>
#include <QString>

namespace Theme {

enum class Style { Clay, HighContrast };
enum class Brightness { Light, Dark, System };

struct Colors {
    QColor window, surface, text, mutedText;    // backgrounds and text
    QColor accent, accentText;                  // buttons, highlights
    QColor border, highlight, shadow;           // edges, raised-top glint, drop shadow
    QColor error;
    QColor high, low, rain, snow;               // chart colors
    QColor markWeather, markNote, markEvent;    // calendar markers
};

// `dark` must already be resolved (System -> Light or Dark).
Colors colors(Style style, bool dark);

QString styleSetting(Style s);
Style styleFromSetting(const QString &text);        // unknown -> Clay
QString brightnessSetting(Brightness b);
Brightness brightnessFromSetting(const QString &text);  // unknown -> System

// WCAG contrast ratio between two colors (1 to 21).
double contrastRatio(const QColor &a, const QColor &b);

} // namespace Theme