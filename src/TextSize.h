#pragma once

#include <QList>
#include <QString>

// Z4: the Text size setting. Every size is offered on every screen; layouts reflow instead
// of being cut off (RZ1-RZ3), so no size is ever hidden.
namespace TextSize {

enum class Size { Normal = 0, Large = 1, ExtraLarge = 2 };

QList<Size> all();                     // Normal, Large, Extra large
double factor(Size s);                 // 1.0, 1.25, 1.5
QString label(Size s);                 // "Normal", "Large", "Extra large"
QString toSetting(Size s);             // "normal", "large", "xlarge"
Size fromSetting(const QString &text); // "largest" (removed) -> Extra large; unknown -> Normal

} // namespace TextSize
