#pragma once

#include <windows.h>

namespace omni::gui::win32 {

INT_PTR CALLBACK main_dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

} // namespace omni::gui::win32
