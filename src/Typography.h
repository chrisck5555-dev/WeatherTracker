#pragma once

#include <QFont>

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
