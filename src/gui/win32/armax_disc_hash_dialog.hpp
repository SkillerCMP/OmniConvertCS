#pragma once

#include <windows.h>

namespace omni::gui::win32 {

struct AppState;

[[nodiscard]] bool show_armax_disc_hash_dialog(HWND owner, AppState& state);

} // namespace omni::gui::win32
