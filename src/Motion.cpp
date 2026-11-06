#include "Motion.h"

#include "Display.h"

namespace Motion {

namespace {
bool g_userEnabled = true;
}

void setUserEnabled(bool on) { g_userEnabled = on; }
bool userEnabled() { return g_userEnabled; }

Level level()
{
    if (!g_userEnabled)
        return Level::Off;
    return Display::animationsAllowed() ? Level::Full : Level::Reduced;
}

int fadeMs(int fullMs) { return level() == Level::Off ? 0 : fullMs; }
int moveMs(int fullMs) { return level() == Level::Full ? fullMs : 0; }

} // namespace Motion
