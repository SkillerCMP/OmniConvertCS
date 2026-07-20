#include "gui/win32/key_dialog.hpp"

#include "devices/action_replay/ar_crypto.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/hex_edit.hpp"
#include "gui/win32/text_control.hpp"
#include "gui/win32/context_controls.hpp"
#include "util/hex.hpp"

#include <array>
#include <string>

namespace omni::gui::win32 {
namespace {

constexpr std::array<const wchar_t*, 4> common_keys{{
    L"1853E59E", L"1645EBB3", L"1746EAAD", L"1456E7A5"
}};

std::uint32_t displayed_key(std::uint32_t seed) noexcept {
    return devices::action_replay::displayed_key_from_seed(seed);
}

std::uint32_t stored_seed(std::uint32_t key) noexcept {
    return devices::action_replay::seed_from_displayed_key(key);
}

void set_key_text(HWND dialog, std::uint32_t key) {
    SetDlgItemTextW(dialog, IDC_EDIT_AR2,
                    utf8_to_utf16(hex::format_u32(key)).c_str());
}

void initialize_common_keys(HWND dialog, std::uint32_t current_key) {
    HWND combo = GetDlgItem(dialog, IDC_AR2_COMMON);
    if (combo == nullptr) return;
    int selected = -1;
    const std::wstring current = utf8_to_utf16(hex::format_u32(current_key));
    for (std::size_t index = 0; index < common_keys.size(); ++index) {
        const LRESULT item = SendMessageW(
            combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(common_keys[index]));
        if (item >= 0 && current == common_keys[index]) {
            selected = static_cast<int>(item);
        }
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

INT_PTR CALLBACK key_dialog_proc(HWND dialog, UINT message, WPARAM wparam,
                                 LPARAM lparam) {
    auto* state = reinterpret_cast<AppState*>(
        GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
        case WM_INITDIALOG: {
            state = reinterpret_cast<AppState*>(lparam);
            SetWindowLongPtrW(dialog, DWLP_USER,
                              reinterpret_cast<LONG_PTR>(state));
            SendDlgItemMessageW(dialog, IDC_EDIT_AR2, EM_SETLIMITTEXT, 8, 0);
            install_hex_edit_filter(GetDlgItem(dialog, IDC_EDIT_AR2));
            if (state != nullptr) {
                const std::uint32_t key = displayed_key(state->ar2_input_key);
                set_key_text(dialog, key);
                initialize_common_keys(dialog, key);
            }
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == IDC_AR2_COMMON &&
                HIWORD(wparam) == CBN_SELCHANGE) {
                HWND combo = GetDlgItem(dialog, IDC_AR2_COMMON);
                const LRESULT selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
                if (selected != CB_ERR) {
                    wchar_t text[32]{};
                    SendMessageW(combo, CB_GETLBTEXT,
                                 static_cast<WPARAM>(selected),
                                 reinterpret_cast<LPARAM>(text));
                    SetDlgItemTextW(dialog, IDC_EDIT_AR2, text);
                    SetFocus(GetDlgItem(dialog, IDC_EDIT_AR2));
                }
                return TRUE;
            }
            switch (LOWORD(wparam)) {
                case ID_BTN_RESET:
                    if (state != nullptr) {
                        state->ar2_input_key = devices::action_replay::ar1_seed;
                    }
                    EndDialog(dialog, IDOK);
                    return TRUE;
                case IDOK: {
                    if (state == nullptr) return TRUE;
                    const std::string text =
                        read_window_text_utf8(GetDlgItem(dialog, IDC_EDIT_AR2));
                    const auto value = hex::parse_u32(text);
                    if (!value || text.size() != 8U) {
                        MessageBoxW(
                            dialog,
                            L"Please enter exactly 8 hexadecimal digits (0-9, A-F).",
                            L"Invalid AR2 key code", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(dialog, IDC_EDIT_AR2));
                        SendDlgItemMessageW(dialog, IDC_EDIT_AR2, EM_SETSEL, 0, -1);
                        return TRUE;
                    }
                    state->ar2_input_key = stored_seed(*value);
                    EndDialog(dialog, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(dialog, IDCANCEL);
                    return TRUE;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

} // namespace

void show_ar2_key_dialog(HWND owner) {
    AppState* state = app_state(owner);
    if (state == nullptr) return;
    DialogBoxParamW(state->instance, MAKEINTRESOURCEW(IDD_DIALOG2), owner,
                    key_dialog_proc, reinterpret_cast<LPARAM>(state));
    refresh_ar2_key_display(owner);
}

} // namespace omni::gui::win32
