#include "gui/win32/hex_edit.hpp"

#include <commctrl.h>

namespace omni::gui::win32 {
namespace {

LRESULT CALLBACK hex_edit_proc(HWND window, UINT message, WPARAM wparam,
                               LPARAM lparam, UINT_PTR subclass_id,
                               DWORD_PTR reference_data) {
    static_cast<void>(reference_data);
    if (message == WM_CHAR) {
        const wchar_t character = static_cast<wchar_t>(wparam);
        const bool control = character < L' ' || character == 0x7FU;
        const bool hex = (character >= L'0' && character <= L'9') ||
                         (character >= L'a' && character <= L'f') ||
                         (character >= L'A' && character <= L'F');
        if (!control && !hex) return 0;
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, hex_edit_proc, subclass_id);
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

} // namespace

void install_hex_edit_filter(HWND edit) noexcept {
    if (edit == nullptr) return;
    SetWindowSubclass(edit, hex_edit_proc, 1U, 0U);
}

} // namespace omni::gui::win32
