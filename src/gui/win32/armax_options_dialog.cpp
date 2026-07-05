#include "gui/win32/armax_options_dialog.hpp"

#include "devices/armax/armax_disc_hash.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/armax_disc_hash_dialog.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/hex_edit.hpp"
#include "gui/win32/text_control.hpp"
#include "util/hex.hpp"

#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <cwctype>
#include <string>

namespace omni::gui::win32 {
namespace {

struct DialogData {
    AppState* state{};
};

constexpr LPARAM disc_hash_none = 0;
constexpr LPARAM disc_hash_manual = 1;
constexpr LPARAM disc_hash_drive_base = 0x100;

void populate_disc_hash(HWND dialog, const AppState& state) {
    HWND combo = GetDlgItem(dialog, IDC_ARMAX_DISC_HASH);
    if (combo == nullptr) return;
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);

    const LRESULT none = SendMessageW(combo, CB_ADDSTRING, 0,
                                      reinterpret_cast<LPARAM>(L"Do Not Create"));
    if (none >= 0) SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(none), disc_hash_none);
    int selected_index = 0;

    if (state.armax_manual_disc_hash) {
        std::wstring label = L"Manual: ";
        label += utf8_to_utf16(hex::format_u32(*state.armax_manual_disc_hash));
        if (!state.armax_disc_hash_game_name.empty()) {
            label += L" - ";
            label += utf8_to_utf16(state.armax_disc_hash_game_name);
        }
        const LRESULT item = SendMessageW(
            combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (item >= 0) {
            SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(item), disc_hash_manual);
            if (state.armax_disc_hash_source == ArmaxDiscHashSource::manual) {
                selected_index = static_cast<int>(item);
            }
        }
    }

    const DWORD drives = GetLogicalDrives();
    for (int index = 0; index < 26; ++index) {
        if ((drives & (1UL << index)) == 0U) continue;
        wchar_t root[] = L"A:\\";
        root[0] = static_cast<wchar_t>(L'A' + index);
        if (GetDriveTypeW(root) != DRIVE_CDROM) continue;

        std::wstring label = L"Use Drive ";
        label += root;
        const LRESULT item = SendMessageW(
            combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (item < 0) continue;
        const char drive = static_cast<char>('A' + index);
        SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(item),
                     static_cast<LPARAM>(disc_hash_drive_base +
                                         static_cast<unsigned char>(drive)));
        if (state.armax_disc_hash_source == ArmaxDiscHashSource::drive &&
            drive == static_cast<char>(std::toupper(
                         static_cast<unsigned char>(state.armax_hash_drive)))) {
            selected_index = static_cast<int>(item);
        }
    }
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected_index), 0);
}

void initialize_dialog(HWND dialog, DialogData& data) {
    AppState& state = *data.state;
    std::uint32_t game_id = state.armax_game_id;
    const std::string main_game_id =
        read_window_text_utf8(GetDlgItem(state.window, IDC_EDIT_GAMEID));
    if (const auto parsed = hex::parse_u32(main_game_id);
        parsed && *parsed <= 0xFFFFU) {
        game_id = *parsed;
    }

    SetDlgItemTextW(dialog, IDC_ARMAX_GAMEID,
                    utf8_to_utf16(hex::format_u32(game_id).substr(4)).c_str());
    SendDlgItemMessageW(dialog, IDC_ARMAX_GAMEID, EM_SETLIMITTEXT, 4, 0);
    install_hex_edit_filter(GetDlgItem(dialog, IDC_ARMAX_GAMEID));

    CheckRadioButton(dialog, IDC_ARMAX_VERIFIER_AUTO,
                     IDC_ARMAX_VERIFIER_MANUAL,
                     state.armax_verifier_mode == convert::ArmaxVerifierMode::manual
                         ? IDC_ARMAX_VERIFIER_MANUAL
                         : IDC_ARMAX_VERIFIER_AUTO);
    const int region_id = state.armax_region == 1U
                              ? IDC_ARMAX_REGION_PAL
                              : state.armax_region == 2U
                                    ? IDC_ARMAX_REGION_JAPAN
                                    : IDC_ARMAX_REGION_USA;
    CheckRadioButton(dialog, IDC_ARMAX_REGION_USA,
                     IDC_ARMAX_REGION_JAPAN, region_id);
    populate_disc_hash(dialog, state);
}

bool commit_dialog(HWND dialog, DialogData& data) {
    const std::string game_id_text =
        read_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_GAMEID));
    const auto game_id = hex::parse_u32(game_id_text);
    if (!game_id || game_id_text.empty() || game_id_text.size() > 4U ||
        *game_id > 0xFFFFU) {
        MessageBoxW(dialog,
                    L"Game ID must be a 4-digit hexadecimal value (0000-FFFF).",
                    L"Invalid Game ID", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(dialog, IDC_ARMAX_GAMEID));
        SendDlgItemMessageW(dialog, IDC_ARMAX_GAMEID, EM_SETSEL, 0, -1);
        return false;
    }

    AppState& state = *data.state;
    state.armax_game_id = *game_id;
    state.armax_verifier_mode =
        IsDlgButtonChecked(dialog, IDC_ARMAX_VERIFIER_MANUAL) == BST_CHECKED
            ? convert::ArmaxVerifierMode::manual
            : convert::ArmaxVerifierMode::automatic;
    state.armax_region =
        IsDlgButtonChecked(dialog, IDC_ARMAX_REGION_PAL) == BST_CHECKED
            ? 1U
            : IsDlgButtonChecked(dialog, IDC_ARMAX_REGION_JAPAN) == BST_CHECKED
                  ? 2U
                  : 0U;

    HWND combo = GetDlgItem(dialog, IDC_ARMAX_DISC_HASH);
    const LRESULT selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    const LRESULT item_data = selected == CB_ERR
                                  ? disc_hash_none
                                  : SendMessageW(combo, CB_GETITEMDATA,
                                                 static_cast<WPARAM>(selected), 0);
    if (item_data == disc_hash_manual && state.armax_manual_disc_hash) {
        state.armax_disc_hash_source = ArmaxDiscHashSource::manual;
        state.armax_hash_drive = '\0';
    } else if (item_data >= disc_hash_drive_base) {
        state.armax_disc_hash_source = ArmaxDiscHashSource::drive;
        state.armax_hash_drive = static_cast<char>(item_data - disc_hash_drive_base);
    } else {
        state.armax_disc_hash_source = ArmaxDiscHashSource::none;
        state.armax_hash_drive = '\0';
    }

    const std::string four = hex::format_u32(state.armax_game_id).substr(4);
    set_window_text_utf8(GetDlgItem(state.window, IDC_EDIT_GAMEID), four);
    return true;
}

INT_PTR CALLBACK dialog_proc(HWND dialog, UINT message,
                             WPARAM wparam, LPARAM lparam) {
    auto* data = reinterpret_cast<DialogData*>(
        GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
        case WM_INITDIALOG:
            data = reinterpret_cast<DialogData*>(lparam);
            SetWindowLongPtrW(dialog, DWLP_USER,
                              reinterpret_cast<LONG_PTR>(data));
            if (data != nullptr && data->state != nullptr) {
                initialize_dialog(dialog, *data);
            }
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDC_ARMAX_DISC_HASH_MANUAL:
                    if (data != nullptr && data->state != nullptr &&
                        HIWORD(wparam) == BN_CLICKED &&
                        show_armax_disc_hash_dialog(dialog, *data->state)) {
                        data->state->armax_disc_hash_source = ArmaxDiscHashSource::manual;
                        populate_disc_hash(dialog, *data->state);
                    }
                    return TRUE;
                case IDOK:
                    if (data != nullptr && data->state != nullptr &&
                        commit_dialog(dialog, *data)) {
                        EndDialog(dialog, IDOK);
                    }
                    return TRUE;
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

bool show_armax_options_dialog(HWND owner) {
    AppState* state = app_state(owner);
    if (state == nullptr) return false;
    DialogData data{state};
    return DialogBoxParamW(state->instance, MAKEINTRESOURCEW(IDD_ARMAX_OPTIONS),
                           owner, dialog_proc,
                           reinterpret_cast<LPARAM>(&data)) == IDOK;
}

std::uint32_t compute_armax_disc_hash(HWND owner, char drive_letter) noexcept {
    if (drive_letter == '\0') return 0U;
    const wchar_t drive = static_cast<wchar_t>(std::towupper(
        static_cast<wint_t>(static_cast<unsigned char>(drive_letter))));
    std::wstring system_path;
    system_path += drive;
    system_path += L":\\SYSTEM.CNF";

    try {
        return devices::armax::compute_disc_hash_from_files(
                   std::filesystem::path(system_path)).hash;
    } catch (const std::exception& error) {
        std::wstring message = L"Could not calculate the AR MAX disc hash:\n";
        message += utf8_to_utf16(error.what());
        MessageBoxW(owner, message.c_str(), L"AR MAX Disc Hash",
                    MB_OK | MB_ICONWARNING);
        return 0U;
    }
}

} // namespace omni::gui::win32
