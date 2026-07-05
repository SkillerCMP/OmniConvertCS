#pragma once

#include <windows.h>

namespace omni::gui::win32 {

void initialize_transpose_mode_menu(HWND window);
[[nodiscard]] bool handle_transpose_mode_command(HWND window, int command_id);

} // namespace omni::gui::win32
