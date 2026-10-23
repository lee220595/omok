#pragma once

#include <windows.h>

namespace omok::gui {
// Owns presentation, input routing and a snapshot-based hint worker; Controller owns the game.
int run(HINSTANCE instance, int show_command);
}
