#pragma once

#include <QList>
#include <QSize>

#include "TextSize.h"

namespace Display {

double currentScaleFactor();            // QT_SCALE_FACTOR of THIS run (1.0 if unset)
bool wasRelaunched();                   // true in a copy started by relaunchWithScale
QSize unscaledAvailableSize();          // screen work area as it would be at scale 1.0
QList<TextSize::Size> allowedTextSizes();

// False when Windows' "Animation effects" is turned off (Settings > Accessibility >
// Visual effects). The program then never animates, whatever its own setting says.
bool animationsAllowed();

// Version 1.1.0 ships with the view animations turned off: they felt choppy. The code stays
// so the 1.2.0 overhaul can replace it. While this is false the Settings check box is hidden
// and the saved "motion" choice is kept as it is.
inline constexpr bool kViewAnimationsInThisVersion = false;

// Start a new copy of the program at this scale. The caller then quits.
bool relaunchWithScale(double factor);

} // namespace Display