#include "gui/win32/app_state.hpp"

namespace omni::gui::win32 {

AppState* app_state(HWND window) noexcept {
    return reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

void set_app_state(HWND window, AppState* state) noexcept {
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
}

} // namespace omni::gui::win32
