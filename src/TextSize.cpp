#include "TextSize.h"

namespace TextSize {

double factor(Size s)
{
    switch (s) {
    case Size::Normal:     return 1.0;
    case Size::Large:      return 1.25;
    case Size::ExtraLarge: return 1.5;
    }
    return 1.0;
}

QString label(Size s)
{
    switch (s) {
    case Size::Normal:     return QStringLiteral("Normal");
    case Size::Large:      return QStringLiteral("Large");
    case Size::ExtraLarge: return QStringLiteral("Extra large");
    }
    return QString();
}

QString toSetting(Size s)
{
    switch (s) {
    case Size::Normal:     return QStringLiteral("normal");
    case Size::Large:      return QStringLiteral("large");
    case Size::ExtraLarge: return QStringLiteral("xlarge");
    }
    return QStringLiteral("normal");
}

Size fromSetting(const QString &text)
{
    if (text == "large")  return Size::Large;
    if (text == "xlarge") return Size::ExtraLarge;
    return Size::Normal;
}

QList<Size> fitting(int availWidth, int availHeight)
{
    QList<Size> result;
    for (Size s : {Size::Normal, Size::Large, Size::ExtraLarge}) {
        const bool fits = kMinWidth * factor(s) <= availWidth
                       && kMinHeight * factor(s) <= availHeight;
        if (s == Size::Normal || fits)
            result << s;
    }
    return result;
}

Size effective(Size saved, const QList<Size> &allowed)
{
    for (int i = static_cast<int>(saved); i >= 0; --i)
        if (allowed.contains(static_cast<Size>(i)))
            return static_cast<Size>(i);
    return Size::Normal;
}

} // namespace TextSize