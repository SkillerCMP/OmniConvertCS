#include "gui/win32/armax_disc_hash_dialog.hpp"

#include "devices/armax/armax_disc_hash.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/hex_edit.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/text_control.hpp"
#include "util/hex.hpp"

#include <array>
#include <cctype>
#include <exception>
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
    std::vector<devices::armax::DiscHashEntry> entries;
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

std::string path_utf8(const std::filesystem::path& path) {
#if defined(_WIN32)
    return path.u8string();
#else
    return path.string();
#endif
}

std::filesystem::path path_from_utf8(const std::string& text) {
    return std::filesystem::u8path(text);
}

void show_message(HWND owner, const std::string& message, UINT flags) {
    const std::wstring wide = utf8_to_utf16(message);
    MessageBoxW(owner, wide.c_str(), L"AR MAX Disc Hash", MB_OK | flags);
}

void set_hash_text(HWND dialog, std::uint32_t hash) {
    set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_VALUE),
                         hex::format_u32(hash));
}

std::optional<std::size_t> selected_entry_index(HWND dialog) {
    const LRESULT selection = SendDlgItemMessageW(dialog, IDC_ARMAX_HASH_KNOWN,
                                                   CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR) return std::nullopt;
    const LRESULT data = SendDlgItemMessageW(dialog, IDC_ARMAX_HASH_KNOWN,
                                              CB_GETITEMDATA,
                                              static_cast<WPARAM>(selection), 0);
    if (data == CB_ERR || data < 0) return std::nullopt;
    return static_cast<std::size_t>(data);
}

void select_entry_by_hash(HWND dialog, std::uint32_t hash) {
    DialogData* data = dialog_data(dialog);
    if (data == nullptr) return;

    const LRESULT count = SendDlgItemMessageW(dialog, IDC_ARMAX_HASH_KNOWN,
                                               CB_GETCOUNT, 0, 0);
    for (LRESULT row = 0; row < count; ++row) {
        const LRESULT item_data = SendDlgItemMessageW(
            dialog, IDC_ARMAX_HASH_KNOWN, CB_GETITEMDATA,
            static_cast<WPARAM>(row), 0);
        if (item_data == CB_ERR || item_data < 0) continue;
        const std::size_t index = static_cast<std::size_t>(item_data);
        if (index < data->entries.size() && data->entries[index].hash == hash) {
            SendDlgItemMessageW(dialog, IDC_ARMAX_HASH_KNOWN, CB_SETCURSEL,
                                static_cast<WPARAM>(row), 0);
            return;
        }
    }
}

void populate_entries(HWND dialog) {
    DialogData* data = dialog_data(dialog);
    if (data == nullptr) return;

    SendDlgItemMessageW(dialog, IDC_ARMAX_HASH_KNOWN, CB_RESETCONTENT, 0, 0);
    data->entries = devices::armax::get_disc_hash_entries(data->mapping_path);
    for (std::size_t index = 0U; index < data->entries.size(); ++index) {
        const std::wstring label = utf8_to_utf16(data->entries[index].display_text());
        const LRESULT row = SendDlgItemMessageW(
            dialog, IDC_ARMAX_HASH_KNOWN, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(label.c_str()));
        if (row != CB_ERR && row != CB_ERRSPACE) {
            SendDlgItemMessageW(dialog, IDC_ARMAX_HASH_KNOWN, CB_SETITEMDATA,
                                static_cast<WPARAM>(row),
                                static_cast<LPARAM>(index));
        }
    }
}

void apply_selected_entry(HWND dialog) {
    DialogData* data = dialog_data(dialog);
    const auto index = selected_entry_index(dialog);
    if (data == nullptr || !index || *index >= data->entries.size()) return;

    const auto& entry = data->entries[*index];
    set_hash_text(dialog, entry.hash);
    set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME),
                         entry.game_name);
    if (!entry.elf_name.empty()) {
        set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_ELF_PATH),
                             entry.elf_name);
    }
}

void compute_selected_files(HWND dialog,
                            const std::optional<std::filesystem::path>& elf_override,
                            bool warn_if_missing) {
    const std::string cnf_text = hex::trim(
        read_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_CNF_PATH)));
    if (cnf_text.empty()) {
        if (warn_if_missing) show_message(dialog, "Select SYSTEM.CNF first.", MB_ICONWARNING);
        return;
    }

    try {
        const auto result = devices::armax::compute_disc_hash_from_files(
            path_from_utf8(cnf_text), elf_override);
        set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_ELF_PATH),
                             path_utf8(result.elf_path));
        set_hash_text(dialog, result.hash);
        select_entry_by_hash(dialog, result.hash);

        DialogData* data = dialog_data(dialog);
        if (data != nullptr) {
            if (const auto known = devices::armax::try_get_disc_hash_game_name(
                    data->mapping_path, result.hash)) {
                set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME), *known);
            } else if (read_window_text_utf8(
                           GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME)).empty()) {
                SetFocus(GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME));
            }
        }
    } catch (const std::exception& error) {
        if (warn_if_missing) {
            show_message(dialog,
                         std::string("Unable to calculate the disc hash:\n") +
                             error.what() +
                             "\n\nIf the ELF is not beside SYSTEM.CNF, select it with Browse ELF.",
                         MB_ICONWARNING);
        }
    }
}

void select_system_cnf(HWND dialog, const std::filesystem::path& path) {
    set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_CNF_PATH), path_utf8(path));
    set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_ELF_PATH), "");
    set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_VALUE), "");
    compute_selected_files(dialog, std::nullopt, true);
}

void select_elf(HWND dialog, const std::filesystem::path& path) {
    set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_ELF_PATH), path_utf8(path));
    compute_selected_files(dialog, path, true);
}

void browse_for_system_cnf(HWND dialog) {
    std::array<wchar_t, 32768> path{};
    static constexpr wchar_t filter[] =
        L"PS2 SYSTEM.CNF (SYSTEM*.CNF)\0SYSTEM*.CNF\0"
        L"CNF files (*.CNF)\0*.CNF\0"
        L"All files (*.*)\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize = sizeof(open);
    open.hwndOwner = dialog;
    open.lpstrFilter = filter;
    open.lpstrFile = path.data();
    open.nMaxFile = static_cast<DWORD>(path.size());
    open.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                 OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&open)) select_system_cnf(dialog, path.data());
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
    if (GetOpenFileNameW(&open)) select_elf(dialog, path.data());
}

bool accept_dialog(HWND dialog) {
    DialogData* data = dialog_data(dialog);
    if (data == nullptr || data->state == nullptr) return false;

    const std::string hash_text = hex::trim(
        read_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_VALUE)));
    const auto hash = hex::parse_u32(hash_text);
    if (!hash || hash_text.size() != 8U || *hash == 0U) {
        show_message(dialog, "Disc hash must be eight hexadecimal digits and non-zero.",
                     MB_ICONWARNING);
        SetFocus(GetDlgItem(dialog, IDC_ARMAX_HASH_VALUE));
        return false;
    }

    const std::string game_name = hex::trim(
        read_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME)));
    if (game_name.empty()) {
        show_message(dialog, "Please enter a game name.", MB_ICONWARNING);
        SetFocus(GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME));
        return false;
    }

    std::string elf_name;
    const std::string elf_text = hex::trim(
        read_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_ELF_PATH)));
    if (!elf_text.empty()) {
        try {
            elf_name = path_from_utf8(elf_text).filename().u8string();
        } catch (...) {
            elf_name = elf_text;
        }
    }

    try {
        devices::armax::add_or_update_disc_hash_entry(
            data->mapping_path, *hash, game_name, elf_name);
    } catch (const std::exception& error) {
        show_message(dialog,
                     std::string("Error saving ArmaxDiscHash.json:\n") + error.what(),
                     MB_ICONERROR);
        return false;
    }

    data->state->armax_disc_hash_source = ArmaxDiscHashSource::manual;
    data->state->armax_manual_disc_hash = *hash;
    data->state->armax_disc_hash_game_name = game_name;
    data->state->armax_disc_hash_elf_name = elf_name;
    set_window_text_utf8(GetDlgItem(data->state->window, IDC_EDIT_GAMENAME), game_name);
    EndDialog(dialog, IDOK);
    return true;
}

INT_PTR CALLBACK disc_hash_dialog_proc(HWND dialog, UINT message,
                                       WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_INITDIALOG: {
            auto* data = reinterpret_cast<DialogData*>(lparam);
            if (data == nullptr || data->state == nullptr) return FALSE;
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(data));
            DragAcceptFiles(dialog, TRUE);
            SendDlgItemMessageW(dialog, IDC_ARMAX_HASH_VALUE, EM_SETLIMITTEXT, 8, 0);
            install_hex_edit_filter(GetDlgItem(dialog, IDC_ARMAX_HASH_VALUE));
            populate_entries(dialog);
            if (data->state->armax_manual_disc_hash) {
                set_hash_text(dialog, *data->state->armax_manual_disc_hash);
                select_entry_by_hash(dialog, *data->state->armax_manual_disc_hash);
            }
            set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME),
                                 data->state->armax_disc_hash_game_name);
            set_window_text_utf8(GetDlgItem(dialog, IDC_ARMAX_HASH_ELF_PATH),
                                 data->state->armax_disc_hash_elf_name);
            if (data->state->armax_disc_hash_game_name.empty()) {
                set_window_text_utf8(
                    GetDlgItem(dialog, IDC_ARMAX_HASH_GAME_NAME),
                    hex::trim(read_window_text_utf8(
                        GetDlgItem(data->state->window, IDC_EDIT_GAMENAME))));
            }
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDC_ARMAX_HASH_BROWSE_CNF:
                    if (HIWORD(wparam) == BN_CLICKED) browse_for_system_cnf(dialog);
                    return TRUE;
                case IDC_ARMAX_HASH_BROWSE_ELF:
                    if (HIWORD(wparam) == BN_CLICKED) browse_for_elf(dialog);
                    return TRUE;
                case IDC_ARMAX_HASH_KNOWN:
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
                const std::filesystem::path dropped(path.data());
                std::string extension = dropped.extension().string();
                for (char& c : extension) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (extension == ".cnf") select_system_cnf(dialog, dropped);
                else select_elf(dialog, dropped);
            }
            DragFinish(drop);
            return TRUE;
        }
        default:
            return FALSE;
    }
}

} // namespace

bool show_armax_disc_hash_dialog(HWND owner, AppState& state) {
    DialogData data;
    data.state = &state;
    data.mapping_path = executable_directory() / "ArmaxDiscHash.json";
    return DialogBoxParamW(state.instance, MAKEINTRESOURCEW(IDD_ARMAX_DISC_HASH),
                           owner, disc_hash_dialog_proc,
                           reinterpret_cast<LPARAM>(&data)) == IDOK;
}

} // namespace omni::gui::win32
