#pragma once

#include <windows.h>

namespace omni::gui::win32 {

void initialize_options_menu(HWND window);
[[nodiscard]] bool handle_options_menu_command(HWND window, int command_id);

} // namespace omni::gui::win32
