#include "TextSize.h"

namespace TextSize {

QList<Size> all() { return {Size::Normal, Size::Large, Size::ExtraLarge, Size::Largest}; }

double factor(Size s)
{
    switch (s) {
    case Size::Normal:     return 1.0;
    case Size::Large:      return 1.25;
    case Size::ExtraLarge: return 1.5;
    case Size::Largest:    return 2.0;
    }
    return 1.0;
}

QString label(Size s)
{
    switch (s) {
    case Size::Normal:     return QStringLiteral("Normal");
    case Size::Large:      return QStringLiteral("Large");
    case Size::ExtraLarge: return QStringLiteral("Extra large");
    case Size::Largest:    return QStringLiteral("Largest");
    }
    return QString();
}

QString toSetting(Size s)
{
    switch (s) {
    case Size::Normal:     return QStringLiteral("normal");
    case Size::Large:      return QStringLiteral("large");
    case Size::ExtraLarge: return QStringLiteral("xlarge");
    case Size::Largest:    return QStringLiteral("largest");
    }
    return QStringLiteral("normal");
}

Size fromSetting(const QString &text)
{
    if (text == "large")   return Size::Large;
    if (text == "xlarge")  return Size::ExtraLarge;
    if (text == "largest") return Size::Largest;
    return Size::Normal;
}

} // namespace TextSize
