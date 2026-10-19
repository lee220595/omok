#pragma once

#include <windows.h>

namespace omok::gui {
// Owns only the native window and message loop. Game logic belongs to other modules.
int run(HINSTANCE instance, int show_command);
}
