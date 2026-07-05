#include "gui/win32/context_controls.hpp"

#include "core/code_format.hpp"
#include "devices/action_replay/ar_crypto.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/hex_edit.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/text_control.hpp"
#include "util/hex.hpp"

#include <string>

namespace omni::gui::win32 {
namespace {

std::uint32_t displayed_ar2_key(std::uint32_t seed) noexcept {
    return devices::action_replay::encrypt_word(seed, 0x05U, 0x18U);
}

void show(HWND window, int id, bool visible) noexcept {
    if (HWND control = GetDlgItem(window, id); control != nullptr) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

} // namespace

void sync_game_id_controls(HWND window, const wchar_t* preferred_text) noexcept {
    AppState* state = app_state(window);
    if (state == nullptr || state->syncing_game_id) return;
    state->syncing_game_id = true;
    std::wstring text;
    if (preferred_text != nullptr && *preferred_text != L'\0') {
        text = preferred_text;
    } else {
        text = utf8_to_utf16(hex::format_u32(state->armax_game_id).substr(4));
    }
    SetDlgItemTextW(window, IDC_EDIT_GAMEID, text.c_str());
    SetDlgItemTextW(window, IDC_EDIT_GAMEID_INPUT, text.c_str());
    SetDlgItemTextW(window, IDC_EDIT_GAMEID_OUTPUT, text.c_str());
    state->syncing_game_id = false;
}

void refresh_ar2_key_display(HWND window) noexcept {
    AppState* state = app_state(window);
    if (state == nullptr) return;
    const bool visible = crypt_for_format(state->input_format) == Crypt::ar2;
    show(window, ID_STATIC_AR2_CURRENT, visible);
    show(window, IDC_EDIT_AR2_CURRENT, visible);
    if (visible) {
        SetDlgItemTextW(window, IDC_EDIT_AR2_CURRENT,
                        utf8_to_utf16(hex::format_u32(displayed_ar2_key(state->ar2_key))).c_str());
    } else {
        SetDlgItemTextW(window, IDC_EDIT_AR2_CURRENT, L"");
    }
}

void refresh_context_controls(HWND window) noexcept {
    AppState* state = app_state(window);
    if (state == nullptr) return;
    const bool input_armax = device_for_format(state->input_format) == Device::armax;
    const bool output_armax = device_for_format(state->output_format) == Device::armax;
    show(window, ID_STATIC_GAMEID_INPUT, input_armax);
    show(window, IDC_EDIT_GAMEID_INPUT, input_armax);
    show(window, ID_STATIC_GAMEID_OUTPUT, output_armax);
    show(window, IDC_EDIT_GAMEID_OUTPUT, output_armax);
    refresh_ar2_key_display(window);
}

void initialize_context_controls(HWND window) noexcept {
    for (const int id : {IDC_EDIT_GAMEID, IDC_EDIT_GAMEID_INPUT, IDC_EDIT_GAMEID_OUTPUT}) {
        HWND edit = GetDlgItem(window, id);
        if (edit != nullptr) {
            SendMessageW(edit, EM_SETLIMITTEXT, 4, 0);
            install_hex_edit_filter(edit);
        }
    }
    if (HWND key = GetDlgItem(window, IDC_EDIT_AR2_CURRENT); key != nullptr) {
        SendMessageW(key, EM_SETREADONLY, TRUE, 0);
    }
    show(window, ID_STATIC_GAMEID, false);
    show(window, IDC_EDIT_GAMEID, false);
    sync_game_id_controls(window);
    refresh_context_controls(window);
}

bool handle_context_control_command(HWND window, int command_id,
                                    int notification_code) noexcept {
    if (notification_code != EN_CHANGE) return false;
    if (command_id != IDC_EDIT_GAMEID && command_id != IDC_EDIT_GAMEID_INPUT &&
        command_id != IDC_EDIT_GAMEID_OUTPUT) return false;
    AppState* state = app_state(window);
    if (state == nullptr || state->syncing_game_id) return true;
    HWND edit = GetDlgItem(window, command_id);
    const std::string text = read_window_text_utf8(edit);
    if (text.empty()) return true;
    const auto value = hex::parse_u32(text);
    if (!value || *value > 0xFFFFU) return true;
    state->armax_game_id = *value;
    const std::wstring wide = utf8_to_utf16(text);
    sync_game_id_controls(window, wide.c_str());
    return true;
}

} // namespace omni::gui::win32
