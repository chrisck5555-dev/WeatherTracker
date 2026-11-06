#pragma once

#include <QList>
#include <QString>

// Z4: the Text size setting. Every size is offered on every screen; layouts reflow instead
// of being cut off (RZ1-RZ3), so no size is ever hidden.
namespace TextSize {

enum class Size { Normal = 0, Large = 1, ExtraLarge = 2, Largest = 3 };

QList<Size> all();                     // Normal, Large, Extra large, Largest
double factor(Size s);                 // 1.0, 1.25, 1.5, 2.0
QString label(Size s);                 // "Normal", "Large", "Extra large", "Largest"
QString toSetting(Size s);             // "normal", "large", "xlarge", "largest"
Size fromSetting(const QString &text); // unknown -> Normal

} // namespace TextSize
