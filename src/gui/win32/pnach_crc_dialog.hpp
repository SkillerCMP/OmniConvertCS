#pragma once

#include <windows.h>

namespace omni::gui::win32 {

[[nodiscard]] bool show_pnach_crc_dialog(HWND owner);
[[nodiscard]] bool handle_pnach_crc_command(HWND window, int command_id,
                                            int notification_code);
void refresh_pnach_crc_controls(HWND window) noexcept;

} // namespace omni::gui::win32
