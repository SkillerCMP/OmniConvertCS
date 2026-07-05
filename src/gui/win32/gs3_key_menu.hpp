#pragma once

#include <windows.h>

namespace omni::gui::win32 {

void initialize_gs3_key_menu(HWND window);
[[nodiscard]] bool handle_gs3_key_command(HWND window, int command_id);

} // namespace omni::gui::win32
