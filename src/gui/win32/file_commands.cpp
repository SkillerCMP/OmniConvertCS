#include "formats/text/copy_paste_cleaner.hpp"
#include "gui/win32/file_commands.hpp"

#include "core/crypt.hpp"
#include "formats/binary/binary_common.hpp"
#include "formats/text/text_cheat_writer.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/format_menu.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/text_control.hpp"
#include "gui/win32/settings.hpp"
#include "util/hex.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cwctype>
#include <optional>
#include <stdexcept>
#include <string>

#include <commdlg.h>
#include <shellapi.h>

namespace omni::gui::win32 {
namespace {

std::optional<std::filesystem::path> select_text_path(HWND owner, bool save) {
    std::array<wchar_t, 32768> path{};
    static constexpr wchar_t open_filter[] =
        L"Supported text / MIPS (*.txt;*.pnach;*.asm;*.s)\0*.txt;*.pnach;*.asm;*.s\0Text files (*.txt)\0*.txt\0MIPS source (*.asm;*.s)\0*.asm;*.s\0PNACH files (*.pnach)\0*.pnach\0All files (*.*)\0*.*\0\0";
    static constexpr wchar_t text_filter[] =
        L"Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    static constexpr wchar_t pnach_filter[] =
        L"PNACH files (*.pnach)\0*.pnach\0Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    static constexpr wchar_t mips_filter[] =
        L"MIPS source (*.asm)\0*.asm\0Assembly source (*.s)\0*.s\0Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";

    bool save_as_pnach = false;
    bool save_as_mips = false;
    if (save) {
        if (const AppState* state = app_state(owner); state != nullptr) {
            save_as_pnach = state->output_format == CodeFormat::pnach_raw;
            save_as_mips = state->output_format == CodeFormat::ps2_mips_r5900;
            if (save_as_pnach) {
                std::string suggested = "PUTNAME.pnach";
                if (state->pnach_crc_active && state->pnach_crc &&
                    !state->pnach_elf_name.empty()) {
                    suggested = state->pnach_elf_name + "_" +
                                hex::format_u32(*state->pnach_crc) + ".pnach";
                }
                const std::wstring wide = utf8_to_utf16(suggested);
                const std::size_t count = std::min(wide.size(), path.size() - 1U);
                std::copy_n(wide.begin(), count, path.begin());
                path[count] = L'\0';
            }
        }
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = save ? (save_as_pnach ? pnach_filter : (save_as_mips ? mips_filter : text_filter)) : open_filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = save_as_pnach ? L"pnach" : (save_as_mips ? L"asm" : L"txt");
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (save) {
        dialog.Flags |= OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    } else {
        dialog.Flags |= OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameW(&dialog)) return std::nullopt;
    }
    return std::filesystem::path(path.data());
}

std::optional<std::filesystem::path> select_application_path(
    HWND owner, bool save, formats::binary::ApplicationFormat format) {
    std::array<wchar_t, 32768> path{};
    const wchar_t* filter = nullptr;
    const wchar_t* extension = nullptr;
    switch (format) {
        case formats::binary::ApplicationFormat::armax_bin: {
            static constexpr wchar_t value[] = L"AR MAX files (*.bin)\0*.bin\0All files (*.*)\0*.*\0\0";
            filter = value;
            extension = L"bin";
            break;
        }
        case formats::binary::ApplicationFormat::codebreaker_cbc: {
            static constexpr wchar_t value[] = L"CodeBreaker files (*.cbc)\0*.cbc\0All files (*.*)\0*.*\0\0";
            filter = value;
            extension = L"cbc";
            break;
        }
        case formats::binary::ApplicationFormat::gameshark_p2m: {
            static constexpr wchar_t value[] = L"XP/GS files (*.p2m)\0*.p2m\0All files (*.*)\0*.*\0\0";
            filter = value;
            extension = L"p2m";
            break;
        }
        case formats::binary::ApplicationFormat::swap_magic_bin: {
            static constexpr wchar_t value[] = L"Swap Magic files (*.bin)\0*.bin\0All files (*.*)\0*.*\0\0";
            filter = value;
            extension = L"bin";
            break;
        }
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = extension;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (save) {
        dialog.Flags |= OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    } else {
        dialog.Flags |= OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameW(&dialog)) return std::nullopt;
    }
    return std::filesystem::path(path.data());
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open the selected file");
    std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    if (bytes.size() >= 3U && static_cast<unsigned char>(bytes[0]) == 0xEFU &&
        static_cast<unsigned char>(bytes[1]) == 0xBBU &&
        static_cast<unsigned char>(bytes[2]) == 0xBFU) {
        bytes.erase(0, 3);
    }
    if (bytes.size() >= 2U && static_cast<unsigned char>(bytes[0]) == 0xFFU &&
        static_cast<unsigned char>(bytes[1]) == 0xFEU) {
        const std::size_t units = (bytes.size() - 2U) / 2U;
        std::wstring wide(units, L'\0');
        for (std::size_t index = 0; index < units; ++index) {
            const auto lo = static_cast<unsigned char>(bytes[2U + index * 2U]);
            const auto hi = static_cast<unsigned char>(bytes[3U + index * 2U]);
            wide[index] = static_cast<wchar_t>(static_cast<unsigned int>(lo) |
                                                (static_cast<unsigned int>(hi) << 8U));
        }
        return utf16_to_utf8(wide);
    }
    return bytes;
}

void write_text_file(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Unable to create the selected file");
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) throw std::runtime_error("Unable to write the selected file");
}

void clear_last_conversion(AppState& state) {
    state.last_converted_blocks.clear();
    state.last_output_format.reset();
    state.last_game_id = 0U;
}

void load_text_path(HWND window, const std::filesystem::path& path) {
    const std::string text = formats::text::clean_copy_paste_text(read_text_file(path));
    set_window_text_utf8(GetDlgItem(window, IDC_EDIT_IN), text);
    if (AppState* state = app_state(window); state != nullptr) clear_last_conversion(*state);
    SetDlgItemTextW(window, ID_STATIC_STATUS, path.filename().c_str());
}

void load_application_path(HWND window, const std::filesystem::path& path,
                           formats::binary::ApplicationFormat format) {
    formats::binary::LoadResult loaded = formats::binary::load_application_file(
        format, path.string());
    set_window_text_utf8(GetDlgItem(window, IDC_EDIT_IN),
                         formats::text::write_text(loaded.blocks,
                                                   crypt_for_format(loaded.input_format)));
    set_window_text_utf8(GetDlgItem(window, IDC_EDIT_OUT), "");
    set_window_text_utf8(GetDlgItem(window, IDC_EDIT_GAMENAME), loaded.game_name);
    if (loaded.game_id != 0U) {
        std::string id = hex::format_u32(loaded.game_id);
        while (id.size() > 4U && id.front() == '0') id.erase(id.begin());
        set_window_text_utf8(GetDlgItem(window, IDC_EDIT_GAMEID), id);
    }
    select_input_format(window, loaded.input_format);
    if (AppState* state = app_state(window); state != nullptr) clear_last_conversion(*state);

    std::wstring status = L"Loaded ";
    status += utf8_to_utf16(formats::binary::application_format_name(format));
    if (!loaded.warnings.empty()) {
        status += L" with warning: ";
        status += utf8_to_utf16(loaded.warnings.front());
    }
    SetDlgItemTextW(window, ID_STATIC_STATUS, status.c_str());
}

bool output_compatible(CodeFormat format, formats::binary::ApplicationFormat application) {
    switch (application) {
        case formats::binary::ApplicationFormat::armax_bin:
            return format == CodeFormat::action_replay_max;
        case formats::binary::ApplicationFormat::codebreaker_cbc:
            return format == CodeFormat::codebreaker1_6 ||
                   format == CodeFormat::codebreaker7_common ||
                   format == CodeFormat::xploder1_3;
        case formats::binary::ApplicationFormat::gameshark_p2m:
            return format == CodeFormat::gameshark_madcatz34 ||
                   format == CodeFormat::gameshark_madcatz5 ||
                   format == CodeFormat::xploder4 || format == CodeFormat::xploder5;
        case formats::binary::ApplicationFormat::swap_magic_bin:
            return format == CodeFormat::standard_raw ||
                   format == CodeFormat::action_replay1 ||
                   format == CodeFormat::gameshark_interact1 ||
                   format == CodeFormat::swap_magic_coder3;
    }
    return false;
}

void show_error(HWND window, const std::exception& error) {
    const std::wstring message = utf8_to_utf16(error.what());
    MessageBoxW(window, message.c_str(), L"OmniConvert", MB_OK | MB_ICONERROR);
}

} // namespace

void open_text_file(HWND window) {
    try {
        const auto path = select_text_path(window, false);
        if (path) load_text_path(window, *path);
    } catch (const std::exception& error) {
        show_error(window, error);
    }
}

void open_application_file(HWND window, formats::binary::ApplicationFormat format) {
    try {
        const auto path = select_application_path(window, false, format);
        if (path) load_application_path(window, *path, format);
    } catch (const std::exception& error) {
        show_error(window, error);
    }
}

void save_output_text(HWND window) {
    try {
        const auto path = select_text_path(window, true);
        if (!path) return;
        const std::string text = read_window_text_utf8(GetDlgItem(window, IDC_EDIT_OUT));
        write_text_file(*path, text);
        SetDlgItemTextW(window, ID_STATIC_STATUS, L"Output saved");
    } catch (const std::exception& error) {
        show_error(window, error);
    }
}

void save_application_file(HWND window, formats::binary::ApplicationFormat format) {
    try {
        AppState* state = app_state(window);
        if (state == nullptr || state->last_converted_blocks.empty() ||
            !state->last_output_format) {
            throw std::invalid_argument("No converted cheats are available. Click Convert first");
        }
        if (!output_compatible(*state->last_output_format, format)) {
            throw std::invalid_argument(
                "The last converted output format is not compatible with " +
                formats::binary::application_format_name(format));
        }
        const auto path = select_application_path(window, true, format);
        if (!path) return;

        formats::binary::WriteOptions options;
        options.game_name = read_window_text_utf8(GetDlgItem(window, IDC_EDIT_GAMENAME));
        options.game_id = state->last_game_id;
        options.cbc_version = state->cbc_version;
        options.include_headings = true;
        formats::binary::write_application_file(format, path->string(),
                                                state->last_converted_blocks, options);
        std::wstring status = L"Saved ";
        status += utf8_to_utf16(formats::binary::application_format_name(format));
        SetDlgItemTextW(window, ID_STATIC_STATUS, status.c_str());
    } catch (const std::exception& error) {
        show_error(window, error);
    }
}

void initialize_cbc_version_menu(HWND window) {
    HMENU menu = GetMenu(window);
    const AppState* state = app_state(window);
    if (menu == nullptr || state == nullptr) return;
    const UINT selected = state->cbc_version == formats::binary::CbcVersion::v8
                              ? ID_OPTIONS_CBC_V8
                              : ID_OPTIONS_CBC_V7;
    CheckMenuRadioItem(menu, ID_OPTIONS_CBC_V7, ID_OPTIONS_CBC_V8, selected,
                       MF_BYCOMMAND);
}

bool handle_cbc_version_command(HWND window, int command_id) {
    if (command_id != ID_OPTIONS_CBC_V7 && command_id != ID_OPTIONS_CBC_V8) return false;
    if (AppState* state = app_state(window); state != nullptr) {
        state->cbc_version = command_id == ID_OPTIONS_CBC_V8
                                 ? formats::binary::CbcVersion::v8
                                 : formats::binary::CbcVersion::v7;
    }
    initialize_cbc_version_menu(window);
    if (AppState* state = app_state(window); state != nullptr) {
        save_app_settings(*state);
    }
    return true;
}

void load_dropped_file(HWND window, HDROP drop) {
    std::array<wchar_t, 32768> path_buffer{};
    if (DragQueryFileW(drop, 0, path_buffer.data(), static_cast<UINT>(path_buffer.size())) > 0U) {
        try {
            const std::filesystem::path path(path_buffer.data());
            std::wstring extension = path.extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
            if (extension == L".cbc") {
                load_application_path(window, path, formats::binary::ApplicationFormat::codebreaker_cbc);
            } else if (extension == L".p2m") {
                load_application_path(window, path, formats::binary::ApplicationFormat::gameshark_p2m);
            } else if (extension == L".bin") {
                const std::vector<std::uint8_t> bytes = formats::binary::common::read_file(path.string());
                const std::string ident(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(
                    std::min<std::size_t>(bytes.size(), 12U)));
                if (ident.rfind("PS2_CODELIST", 0U) == 0U) {
                    load_application_path(window, path, formats::binary::ApplicationFormat::armax_bin);
                } else {
                    try {
                        static_cast<void>(formats::binary::load_swap_magic_bin(bytes));
                        load_application_path(window, path,
                            formats::binary::ApplicationFormat::swap_magic_bin);
                    } catch (const std::exception& swap_error) {
                        throw std::invalid_argument(
                            "This BIN file is neither an AR MAX PS2_CODELIST nor a valid "
                            "Swap Magic BIN file: " + std::string(swap_error.what()));
                    }
                }
            } else {
                load_text_path(window, path);
            }
        } catch (const std::exception& error) {
            show_error(window, error);
        }
    }
    DragFinish(drop);
}

} // namespace omni::gui::win32
