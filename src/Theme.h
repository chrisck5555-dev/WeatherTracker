#pragma once

#include <QColor>
#include <QString>

namespace Theme {

enum class Style { Clay, HighContrast };
enum class Brightness { Light, Dark, System };

struct Colors {
    QColor window, surface, text, mutedText;    // backgrounds and text
    QColor raised;                              // glass and other raised surfaces (G3, E4)
    QColor disabledText;                        // ST5: disabled labels and outlines, 4.5:1
    QColor accent, accentText;                  // buttons, highlights
    QColor border, highlight, shadow;           // edges, raised-top glint, drop shadow
    QColor inputBorder;                         // outline of text boxes: 3:1 so they can be found
    QColor error;
    QColor high, low, rain, snow;               // chart colors (never the accent, C5)
    QColor markWeather, markNote, markEvent;    // calendar markers
};

// `dark` must already be resolved (System -> Light or Dark).
Colors colors(Style style, bool dark);

// An opaque mix: `amount` of `top` over `base` (0 = base, 1 = top).
QColor mix(const QColor &base, const QColor &top, double amount);

// State overlays, drawn over a control's fill (ST2, ST3, F1, F4).
// Hover and pressed tint toward the main text colour on every control, which also keeps the
// label on an accent fill readable. `fill` is the colour under the overlay and `label` the
// text drawn on it.
// Hover and pressed (ST2, ST3): a tint toward the text colour. On the look's surface it is
// 8% (hover) or 10% (pressed). Given the fill underneath, the tint is set so that fill's
// lightness changes as much as the surface's does (C7), so hover shows as clearly on an
// accent-filled button as on a card-coloured one (D58).
QColor hoverOverlay(const Colors &c, const QColor &fill = QColor());
QColor pressOverlay(const Colors &c, const QColor &fill = QColor());
double lightness(const QColor &c);                          // CIELAB L*, 0 (black) to 100
// Keyboard focus: 30% accent (16% in high contrast) on ordinary controls. On an accent fill
// the accent can't show, so the tint uses the text colour or the on-accent text colour,
// whichever keeps the label at C1's minimum and changes the fill more.
QColor focusOverlay(Style style, const Colors &c, bool onAccent);

// DLG7: the 30% black behind a dialog that needs an answer.
QColor smoke();

QString styleSetting(Style s);
Style styleFromSetting(const QString &text);        // unknown -> Clay
QString brightnessSetting(Brightness b);
Brightness brightnessFromSetting(const QString &text);  // unknown -> System

// WCAG contrast ratio between two colors (1 to 21).
double contrastRatio(const QColor &a, const QColor &b);

} // namespace Theme