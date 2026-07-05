#pragma once

#include <string>
#include <string_view>

#include <windows.h>

namespace omni::gui::win32 {

[[nodiscard]] std::string read_window_text_utf8(HWND control);
void set_window_text_utf8(HWND control, std::string_view text);
[[nodiscard]] std::wstring utf8_to_utf16(std::string_view text);
[[nodiscard]] std::string utf16_to_utf8(std::wstring_view text);

} // namespace omni::gui::win32
