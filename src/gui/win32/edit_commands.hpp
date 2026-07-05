#pragma once

#include <windows.h>

namespace omni::gui::win32 {

// Installs a WM_PASTE interceptor on the main input editor so clipboard text
// using CRLF, LF, standalone CR, or mixed line endings is shown correctly.
void install_input_paste_normalizer(HWND window) noexcept;

bool handle_edit_command(HWND window, int command_id) noexcept;

} // namespace omni::gui::win32
