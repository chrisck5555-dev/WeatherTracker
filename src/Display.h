#pragma once

#include <QList>
#include <QSize>

#include "TextSize.h"

namespace Display {

double currentScaleFactor();            // QT_SCALE_FACTOR of THIS run (1.0 if unset)
bool wasRelaunched();                   // true in a copy started by relaunchWithScale
QSize unscaledAvailableSize();          // screen work area as it would be at scale 1.0
QList<TextSize::Size> allowedTextSizes();

// Start a new copy of the program at this scale. The caller then quits.
bool relaunchWithScale(double factor);

} // namespace Display