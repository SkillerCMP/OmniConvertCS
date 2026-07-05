#pragma once

#include <windows.h>

namespace omni::gui::win32 {

void run_conversion(HWND window);
void swap_input_output(HWND window);
void set_status(HWND window, const wchar_t* message) noexcept;

} // namespace omni::gui::win32
