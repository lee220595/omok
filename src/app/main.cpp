#include "main_window.hpp"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command)
{
    return omok::gui::run(instance, show_command);
}
