#pragma once

#include "core/code_format.hpp"

#include <optional>

#include <windows.h>

namespace omni::gui::win32 {

struct MenuFormatSelection {
    bool is_input{};
    CodeFormat format{CodeFormat::standard_raw};
};

void initialize_format_menus(HWND window);
[[nodiscard]] std::optional<MenuFormatSelection> menu_format_selection(int command_id) noexcept;
void select_input_format(HWND window, CodeFormat format);
void select_output_format(HWND window, CodeFormat format);
[[nodiscard]] const wchar_t* format_label(CodeFormat format) noexcept;

} // namespace omni::gui::win32
