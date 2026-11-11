#pragma once

#include <QList>
#include <QSize>

#include "TextSize.h"

namespace Display {

double currentScaleFactor();            // QT_SCALE_FACTOR of THIS run (1.0 if unset)
bool wasRelaunched();                   // true in a copy started by relaunchWithScale

// False when Windows' "Animation effects" is turned off (Settings > Accessibility >
// Visual effects). The program then uses Reduced motion (Motion.h, M19).
bool animationsAllowed();

// G9: glass turns solid when Windows' "Transparency effects" is off (Settings >
// Personalization > Colors) or battery saver is on. Both are false off Windows.
bool transparencyOff();
bool batterySaverOn();

// SC2 (D59): the width Windows gives scroll bars, in layout px (as it would be at 100%):
// 17 unless she changed it. 17 off Windows.
int scrollBarWidth();

// Version 1.1.0 ships with the view animations turned off: they felt choppy. The code stays
// so the 1.2.0 overhaul can replace it. While this is false the Settings check box is hidden
// and the saved "motion" choice is kept as it is.
inline constexpr bool kViewAnimationsInThisVersion = false;

// Start a new copy of the program at this scale. The caller then quits.
bool relaunchWithScale(double factor);

} // namespace Display
