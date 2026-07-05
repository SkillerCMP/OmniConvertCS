#pragma once
#include <windows.h>
namespace omni::gui::win32 {
void initialize_context_controls(HWND window) noexcept;
void refresh_context_controls(HWND window) noexcept;
void refresh_ar2_key_display(HWND window) noexcept;
void sync_game_id_controls(HWND window, const wchar_t* preferred_text = nullptr) noexcept;
bool handle_context_control_command(HWND window, int command_id, int notification_code) noexcept;
}
