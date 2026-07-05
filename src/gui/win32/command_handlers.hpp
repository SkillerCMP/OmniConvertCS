#pragma once

#include <windows.h>

namespace omni::gui::win32 {

bool handle_command(HWND window, int command_id, int notification_code);

} // namespace omni::gui::win32
