#include "gui/win32/context_controls.hpp"

#include "core/code_format.hpp"
#include "devices/action_replay/ar_crypto.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/hex_edit.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/text_control.hpp"
#include "util/hex.hpp"

#include <array>
#include <string>

namespace omni::gui::win32 {
namespace {

constexpr std::array<const wchar_t*, 4> common_ar2_keys{{
    L"1853E59E", L"1645EBB3", L"1746EAAD", L"1456E7A5"
}};

std::uint32_t displayed_ar2_key(std::uint32_t seed) noexcept {
    return devices::action_replay::displayed_key_from_seed(seed);
}

void show(HWND window, int id, bool visible) noexcept {
    if (HWND control = GetDlgItem(window, id); control != nullptr) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

void fill_ar2_key_combo(HWND combo, std::uint32_t seed) noexcept {
    if (combo == nullptr) return;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    const std::wstring current = utf8_to_utf16(hex::format_u32(displayed_ar2_key(seed)));
    int selected = -1;

    for (const wchar_t* key : common_ar2_keys) {
        const LRESULT item = SendMessageW(combo, CB_ADDSTRING, 0,
                                          reinterpret_cast<LPARAM>(key));
        if (item >= 0 && current == key) selected = static_cast<int>(item);
    }

    if (selected < 0) {
        const LRESULT item = SendMessageW(combo, CB_ADDSTRING, 0,
                                          reinterpret_cast<LPARAM>(current.c_str()));
        if (item >= 0) selected = static_cast<int>(item);
    }

    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

bool apply_ar2_combo_selection(HWND window, int combo_id) noexcept {
    AppState* state = app_state(window);
    if (state == nullptr) return true;

    HWND combo = GetDlgItem(window, combo_id);
    if (combo == nullptr) return true;

    std::string key_text = read_window_text_utf8(combo);
    if (key_text.empty()) {
        const LRESULT selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        if (selected == CB_ERR) return true;

        wchar_t text[32]{};
        SendMessageW(combo, CB_GETLBTEXT, static_cast<WPARAM>(selected),
                     reinterpret_cast<LPARAM>(text));
        key_text = utf16_to_utf8(text);
    }
    if (key_text.size() != 8U) return true;
    const auto displayed = hex::parse_u32(key_text);
    if (!displayed) return true;

    const std::uint32_t seed =
        devices::action_replay::seed_from_displayed_key(*displayed);
    if (combo_id == IDC_EDIT_AR2_CURRENT) {
        state->ar2_input_key = seed;
    } else if (combo_id == IDC_AR2_OUTPUT_CURRENT) {
        state->ar2_output_key = seed;
    }
    return true;
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

    const bool input_visible = crypt_for_format(state->input_format) == Crypt::ar2;
    const bool output_visible = crypt_for_format(state->output_format) == Crypt::ar2;

    show(window, ID_STATIC_AR2_CURRENT, input_visible);
    show(window, IDC_EDIT_AR2_CURRENT, input_visible);
    show(window, ID_STATIC_AR2_OUTPUT_CURRENT, output_visible);
    show(window, IDC_AR2_OUTPUT_CURRENT, output_visible);

    if (input_visible) {
        fill_ar2_key_combo(GetDlgItem(window, IDC_EDIT_AR2_CURRENT), state->ar2_input_key);
    }
    if (output_visible) {
        fill_ar2_key_combo(GetDlgItem(window, IDC_AR2_OUTPUT_CURRENT), state->ar2_output_key);
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
    show(window, ID_STATIC_GAMEID, false);
    show(window, IDC_EDIT_GAMEID, false);
    sync_game_id_controls(window);
    refresh_context_controls(window);
}

bool handle_context_control_command(HWND window, int command_id,
                                    int notification_code) noexcept {
    if ((command_id == IDC_EDIT_AR2_CURRENT || command_id == IDC_AR2_OUTPUT_CURRENT) &&
        (notification_code == CBN_SELCHANGE ||
         notification_code == CBN_EDITCHANGE ||
         notification_code == CBN_KILLFOCUS)) {
        return apply_ar2_combo_selection(window, command_id);
    }

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
