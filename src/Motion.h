#pragma once

// How much the program may animate (M11, M19).
//   Full:    everything in the motion plan
//   Reduced: Windows' "Animation effects" is off: short fades only, nothing moves
//   Off:     the Settings check box is off: everything is instant
namespace Motion {

enum class Level { Full, Reduced, Off };

void setUserEnabled(bool on);       // the Settings check box (saved as "motion")
bool userEnabled();
Level level();                      // the check box combined with Windows' setting

// Durations to use for a fade or a movement at the current level (0 = instant).
int fadeMs(int fullMs);             // Full: fullMs; Reduced: fullMs; Off: 0
int moveMs(int fullMs);             // Full: fullMs; Reduced and Off: 0

} // namespace Motion
