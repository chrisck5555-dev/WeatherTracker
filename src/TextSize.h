#pragma once

#include <QList>
#include <QString>

namespace TextSize {

enum class Size { Normal = 0, Large = 1, ExtraLarge = 2 };

// The window's design size (logical pixels) at Normal.
constexpr int kMinWidth = 1000;
constexpr int kMinHeight = 650;

double factor(Size s);                 // 1.0, 1.25, 1.5
QString label(Size s);                 // "Normal", "Large", "Extra large"
QString toSetting(Size s);             // "normal", "large", "xlarge"
Size fromSetting(const QString &text); // unknown -> Normal

// Sizes whose scaled window fits in availWidth x availHeight (UNSCALED logical pixels).
// Normal is always included.
QList<Size> fitting(int availWidth, int availHeight);

// The saved size if it fits, otherwise the largest allowed size below it.
Size effective(Size saved, const QList<Size> &allowed);

} // namespace TextSize