#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QString>

#include "Design.h"

// Digits that are all the same width ("tabular figures"), so numbers in a column line up:
// 11.1 and 18.8 take the same space. Needs Qt 6.7 or newer; on older Qt the font is
// returned unchanged.
inline QFont tabularDigits(QFont f)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    f.setFeature(QFont::Tag("tnum"), 1);
#endif
    return f;
}

// One of the four type sizes (T6), in pixels, Regular or Semibold. `base` keeps the family
// (the Windows system font, T1); by default it is the application's font.
inline QFont textFont(Design::TextStyle style, QFont base = QFont())
{
    const Design::TypeSpec t = Design::type(style);
    base.setPixelSize(t.px);
    base.setWeight(t.semibold ? QFont::DemiBold : QFont::Normal);
    return base;
}

// T1, T6: the app's own font: the Windows system font (Segoe UI Variable on Windows 11,
// Segoe UI on Windows 10) at the Body size, 18 px, for everything that doesn't choose its
// own. main.cpp and the layout check both use it, so the check measures what she sees.
inline QFont appFont(QFont system)
{
    if (QFontDatabase::hasFamily(QStringLiteral("Segoe UI Variable Text")))
        system.setFamily(QStringLiteral("Segoe UI Variable Text"));
    return textFont(Design::TextStyle::Body, system);
}

// Semibold for emphasis in Body text (T6: no Bold).
inline QFont semibold(QFont f)
{
    f.setWeight(QFont::DemiBold);
    return f;
}

// The same, as a stylesheet fragment for widgets that are styled with one.
inline QString textStyleSheet(Design::TextStyle style)
{
    const Design::TypeSpec t = Design::type(style);
    return QString("font-size: %1px; font-weight: %2;").arg(t.px).arg(t.semibold ? 600 : 400);
}
