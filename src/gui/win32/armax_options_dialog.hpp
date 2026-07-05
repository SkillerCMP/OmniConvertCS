#pragma once

#include <cstdint>

#include <windows.h>

namespace omni::gui::win32 {

[[nodiscard]] bool show_armax_options_dialog(HWND owner);
[[nodiscard]] std::uint32_t compute_armax_disc_hash(HWND owner, char drive_letter) noexcept;

} // namespace omni::gui::win32
