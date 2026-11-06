#include "Responsive.h"

#include "Design.h"

namespace Responsive {

Band band(int layoutWidth)
{
    if (layoutWidth >= Design::kWideMinWidth)
        return Band::Wide;
    if (layoutWidth >= Design::kMediumMinWidth)
        return Band::Medium;
    return Band::Narrow;
}

QString bandName(Band b)
{
    switch (b) {
    case Band::Narrow: return QStringLiteral("Narrow");
    case Band::Medium: return QStringLiteral("Medium");
    case Band::Wide:   return QStringLiteral("Wide");
    }
    return QString();
}

} // namespace Responsive
