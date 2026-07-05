#include "gui/win32/pnach_crc_dialog.hpp"

#include "formats/pnach/pnach_crc.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/text_control.hpp"
#include "util/hex.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <commdlg.h>
#include <shellapi.h>

namespace omni::gui::win32 {
namespace {

struct DialogData {
    AppState* state{};
    std::filesystem::path mapping_path;
    std::vector<formats::pnach::CrcEntry> entries;
};

DialogData* dialog_data(HWND dialog) noexcept {
    return reinterpret_cast<DialogData*>(GetWindowLongPtrW(dialog, DWLP_USER));
}

std::filesystem::path executable_directory() {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0U || length >= path.size()) return std::filesystem::current_path();
    return std::filesystem::path(path.data()).parent_path();
}

void show_message(HWND owner, const std::string& message, UINT flags) {
    const std::wstring wide = utf8_to_utf16(message);
    MessageBoxW(owner, wide.c_str(), L"PNACH CRC", MB_OK | flags);
}

void set_crc_text(HWND dialog, std::uint32_t crc) {
    set_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_CRC), hex::format_u32(crc));
}

std::optional<std::size_t> selected_entry_index(HWND dialog) {
    const LRESULT selection = SendDlgItemMessageW(dialog, IDC_PNACH_KNOWN,
                                                   CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR) return std::nullopt;
    const LRESULT data = SendDlgItemMessageW(dialog, IDC_PNACH_KNOWN,
                                              CB_GETITEMDATA,
                                              static_cast<WPARAM>(selection), 0);
    if (data == CB_ERR || data < 0) return std::nullopt;
    return static_cast<std::size_t>(data);
}

void select_entry_by_crc(HWND dialog, std::uint32_t crc) {
    DialogData* data = dialog_data(dialog);
    if (data == nullptr) return;

    const LRESULT count = SendDlgItemMessageW(dialog, IDC_PNACH_KNOWN,
                                               CB_GETCOUNT, 0, 0);
    for (LRESULT row = 0; row < count; ++row) {
        const LRESULT item_data = SendDlgItemMessageW(
            dialog, IDC_PNACH_KNOWN, CB_GETITEMDATA,
            static_cast<WPARAM>(row), 0);
        if (item_data == CB_ERR || item_data < 0) continue;
        const std::size_t index = static_cast<std::size_t>(item_data);
        if (index < data->entries.size() && data->entries[index].crc == crc) {
            SendDlgItemMessageW(dialog, IDC_PNACH_KNOWN, CB_SETCURSEL,
                                static_cast<WPARAM>(row), 0);
            return;
        }
    }
}

void apply_selected_entry(HWND dialog) {
    DialogData* data = dialog_data(dialog);
    const auto index = selected_entry_index(dialog);
    if (data == nullptr || !index || *index >= data->entries.size()) return;

    const formats::pnach::CrcEntry& entry = data->entries[*index];
    set_crc_text(dialog, entry.crc);
    set_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_GAME_NAME), entry.game_name);
}

void populate_entries(HWND dialog) {
    DialogData* data = dialog_data(dialog);
    if (data == nullptr) return;

    SendDlgItemMessageW(dialog, IDC_PNACH_KNOWN, CB_RESETCONTENT, 0, 0);
    data->entries = formats::pnach::get_crc_entries(data->mapping_path);
    for (std::size_t index = 0U; index < data->entries.size(); ++index) {
        const std::wstring label = utf8_to_utf16(data->entries[index].display_text());
        const LRESULT row = SendDlgItemMessageW(
            dialog, IDC_PNACH_KNOWN, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(label.c_str()));
        if (row != CB_ERR && row != CB_ERRSPACE) {
            SendDlgItemMessageW(dialog, IDC_PNACH_KNOWN, CB_SETITEMDATA,
                                static_cast<WPARAM>(row),
                                static_cast<LPARAM>(index));
        }
    }
}

void compute_selected_elf(HWND dialog, const std::filesystem::path& path) {
    DialogData* data = dialog_data(dialog);
    if (data == nullptr || path.empty()) return;

    set_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_ELF_PATH), path.string());
    try {
        const std::uint32_t crc = formats::pnach::compute_elf_crc(path);
        set_crc_text(dialog, crc);
        select_entry_by_crc(dialog, crc);

        if (const auto known = formats::pnach::try_get_game_name(data->mapping_path, crc)) {
            set_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_GAME_NAME), *known);
        } else if (read_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_GAME_NAME)).empty()) {
            SetFocus(GetDlgItem(dialog, IDC_PNACH_GAME_NAME));
        }
    } catch (const std::exception& error) {
        show_message(dialog, std::string("Error computing CRC for ELF:\n") + error.what(),
                     MB_ICONERROR);
    }
}

void browse_for_elf(HWND dialog) {
    std::array<wchar_t, 32768> path{};
    static constexpr wchar_t filter[] =
        L"PS2 ELF and executables (*.ELF;SLUS_*;SLES_*;SCES_*;SCUS_*;SLPS_*;SLPM_*)\0"
        L"*.ELF;SLUS_*;SLES_*;SCES_*;SCUS_*;SLPS_*;SLPM_*\0"
        L"All files (*.*)\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize = sizeof(open);
    open.hwndOwner = dialog;
    open.lpstrFilter = filter;
    open.lpstrFile = path.data();
    open.nMaxFile = static_cast<DWORD>(path.size());
    open.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                 OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&open)) compute_selected_elf(dialog, path.data());
}

bool accept_dialog(HWND dialog) {
    DialogData* data = dialog_data(dialog);
    if (data == nullptr || data->state == nullptr) return false;

    const std::string crc_text = hex::trim(
        read_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_CRC)));
    const auto crc = hex::parse_u32(crc_text);
    if (!crc || crc_text.empty() || crc_text.size() > 8U) {
        show_message(dialog, "CRC must be 8-digit hexadecimal.", MB_ICONWARNING);
        SetFocus(GetDlgItem(dialog, IDC_PNACH_CRC));
        return false;
    }

    const std::string game_name = hex::trim(
        read_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_GAME_NAME)));
    if (game_name.empty()) {
        show_message(dialog, "Please enter a game name.", MB_ICONWARNING);
        SetFocus(GetDlgItem(dialog, IDC_PNACH_GAME_NAME));
        return false;
    }

    std::string elf_name;
    if (const auto index = selected_entry_index(dialog);
        index && *index < data->entries.size() &&
        !data->entries[*index].elf_name.empty()) {
        elf_name = data->entries[*index].elf_name;
    } else {
        const std::string path_text = read_window_text_utf8(
            GetDlgItem(dialog, IDC_PNACH_ELF_PATH));
        if (!path_text.empty()) {
            try {
                elf_name = std::filesystem::path(path_text).filename().string();
            } catch (...) {
                elf_name = hex::trim(path_text);
            }
        }
    }
    elf_name = formats::pnach::normalize_elf_name(elf_name);

    try {
        formats::pnach::add_or_update_crc_entry(data->mapping_path, *crc,
                                                 game_name, elf_name);
    } catch (const std::exception& error) {
        show_message(dialog, std::string("Error saving PnachCRC.json:\n") + error.what(),
                     MB_ICONERROR);
    }

    data->state->pnach_crc = *crc;
    data->state->pnach_game_name = game_name;
    data->state->pnach_elf_name = elf_name;
    set_window_text_utf8(GetDlgItem(data->state->window, IDC_EDIT_GAMENAME), game_name);
    EndDialog(dialog, IDOK);
    return true;
}

INT_PTR CALLBACK pnach_crc_dialog_proc(HWND dialog, UINT message,
                                       WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_INITDIALOG: {
            auto* data = reinterpret_cast<DialogData*>(lparam);
            if (data == nullptr || data->state == nullptr) return FALSE;
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(data));
            DragAcceptFiles(dialog, TRUE);
            populate_entries(dialog);
            if (data->state->pnach_crc) {
                set_crc_text(dialog, *data->state->pnach_crc);
                select_entry_by_crc(dialog, *data->state->pnach_crc);
            }
            set_window_text_utf8(GetDlgItem(dialog, IDC_PNACH_GAME_NAME),
                                 data->state->pnach_game_name);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDC_PNACH_BROWSE:
                    if (HIWORD(wparam) == BN_CLICKED) browse_for_elf(dialog);
                    return TRUE;
                case IDC_PNACH_KNOWN:
                    if (HIWORD(wparam) == CBN_SELCHANGE) apply_selected_entry(dialog);
                    return TRUE;
                case IDOK:
                    if (HIWORD(wparam) == BN_CLICKED) (void)accept_dialog(dialog);
                    return TRUE;
                case IDCANCEL:
                    EndDialog(dialog, IDCANCEL);
                    return TRUE;
                default:
                    return FALSE;
            }
        case WM_DROPFILES: {
            std::array<wchar_t, 32768> path{};
            const auto drop = reinterpret_cast<HDROP>(wparam);
            if (DragQueryFileW(drop, 0, path.data(), static_cast<UINT>(path.size())) > 0U) {
                compute_selected_elf(dialog, path.data());
            }
            DragFinish(drop);
            return TRUE;
        }
        default:
            return FALSE;
    }
}

} // namespace

bool show_pnach_crc_dialog(HWND owner) {
    AppState* state = app_state(owner);
    if (state == nullptr) return false;

    if (state->pnach_game_name.empty()) {
        state->pnach_game_name = hex::trim(
            read_window_text_utf8(GetDlgItem(owner, IDC_EDIT_GAMENAME)));
    }

    DialogData data;
    data.state = state;
    data.mapping_path = executable_directory() / "PnachCRC.json";
    const INT_PTR result = DialogBoxParamW(
        state->instance, MAKEINTRESOURCEW(IDD_PNACH_CRC), owner,
        pnach_crc_dialog_proc, reinterpret_cast<LPARAM>(&data));
    refresh_pnach_crc_controls(owner);
    return result == IDOK;
}

void refresh_pnach_crc_controls(HWND window) noexcept {
    AppState* state = app_state(window);
    if (state == nullptr) return;
    const bool visible = state->output_format == CodeFormat::pnach_raw;
    HWND check = GetDlgItem(window, IDC_CHECK_PNACH_CRC);
    ShowWindow(check, visible ? SW_SHOW : SW_HIDE);
    SendMessageW(check, BM_SETCHECK,
                 state->pnach_crc_active && state->pnach_crc
                     ? BST_CHECKED : BST_UNCHECKED,
                 0);
}

bool handle_pnach_crc_command(HWND window, int command_id,
                              int notification_code) {
    AppState* state = app_state(window);
    if (state == nullptr) return false;

    if (command_id == ID_OPTIONS_PNACH_CRC) {
        (void)show_pnach_crc_dialog(window);
        return true;
    }
    if (command_id != IDC_CHECK_PNACH_CRC || notification_code != BN_CLICKED) {
        return false;
    }

    HWND check = GetDlgItem(window, IDC_CHECK_PNACH_CRC);
    const bool checked = SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!checked) {
        state->pnach_crc_active = false;
        return true;
    }

    state->pnach_crc_active = true;
    (void)show_pnach_crc_dialog(window);
    if (!state->pnach_crc) state->pnach_crc_active = false;
    refresh_pnach_crc_controls(window);
    return true;
}

} // namespace omni::gui::win32
