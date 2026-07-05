#include "gui/win32/edit_commands.hpp"

#include "gui/win32/resources/resource.h"
#include "formats/text/copy_paste_cleaner.hpp"
#include "gui/win32/text_control.hpp"
#include "util/newlines.hpp"

#include <commctrl.h>

#include <string>

namespace omni::gui::win32 {
namespace {

constexpr UINT_PTR input_edit_subclass_id = 1U;
constexpr UINT_PTR output_edit_subclass_id = 2U;

HWND active_edit(HWND window) noexcept {
    HWND focus = GetFocus();
    if (focus == GetDlgItem(window, IDC_EDIT_IN) || focus == GetDlgItem(window, IDC_EDIT_OUT) ||
        focus == GetDlgItem(window, IDC_EDIT_GAMEID) ||
        focus == GetDlgItem(window, IDC_EDIT_GAMEID_INPUT) ||
        focus == GetDlgItem(window, IDC_EDIT_GAMEID_OUTPUT) ||
        focus == GetDlgItem(window, IDC_EDIT_GAMENAME)) {
        return focus;
    }
    return GetDlgItem(window, IDC_EDIT_IN);
}

bool read_clipboard_text(HWND owner, std::wstring& output) noexcept {
    if (OpenClipboard(owner) == FALSE) return false;

    bool success = false;
    if (HANDLE handle = GetClipboardData(CF_UNICODETEXT); handle != nullptr) {
        const auto* data = static_cast<const wchar_t*>(GlobalLock(handle));
        if (data != nullptr) {
            const SIZE_T character_count = GlobalSize(handle) / sizeof(wchar_t);
            std::wstring_view view(data, character_count);
            if (const std::size_t terminator = view.find(L'\0'); terminator != std::wstring_view::npos) {
                view = view.substr(0U, terminator);
            }
            output.assign(view.begin(), view.end());
            GlobalUnlock(handle);
            success = true;
        }
    } else if (HANDLE ansi_handle = GetClipboardData(CF_TEXT); ansi_handle != nullptr) {
        const auto* data = static_cast<const char*>(GlobalLock(ansi_handle));
        if (data != nullptr) {
            const SIZE_T byte_count = GlobalSize(ansi_handle);
            std::string_view view(data, byte_count);
            if (const std::size_t terminator = view.find('\0'); terminator != std::string_view::npos) {
                view = view.substr(0U, terminator);
            }
            try {
                output = utf8_to_utf16(view);
                success = true;
            } catch (...) {
                success = false;
            }
            GlobalUnlock(ansi_handle);
        }
    }

    CloseClipboard();
    return success;
}

LRESULT CALLBACK input_edit_subclass_proc(HWND edit, UINT message, WPARAM wparam,
                                         LPARAM lparam, UINT_PTR subclass_id,
                                         DWORD_PTR reference_data) {
    (void)reference_data;

    if (message == WM_PASTE) {
        std::wstring clipboard_text;
        if (read_clipboard_text(edit, clipboard_text)) {
            std::wstring normalized;
            if (subclass_id == input_edit_subclass_id) {
                try {
                    const std::string cleaned =
                        formats::text::clean_copy_paste_text(utf16_to_utf8(clipboard_text));
                    normalized = utf8_to_utf16(cleaned);
                } catch (...) {
                    normalized = newlines::to_crlf(clipboard_text);
                }
            } else {
                normalized = newlines::to_crlf(clipboard_text);
            }
            SendMessageW(edit, EM_REPLACESEL, TRUE,
                         reinterpret_cast<LPARAM>(normalized.c_str()));
            return 0;
        }
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(edit, input_edit_subclass_proc, subclass_id);
    }

    return DefSubclassProc(edit, message, wparam, lparam);
}

} // namespace

void install_input_paste_normalizer(HWND window) noexcept {
    HWND input = GetDlgItem(window, IDC_EDIT_IN);
    if (input != nullptr) SetWindowSubclass(input, input_edit_subclass_proc, input_edit_subclass_id, 0U);
    HWND output = GetDlgItem(window, IDC_EDIT_OUT);
    if (output != nullptr) SetWindowSubclass(output, input_edit_subclass_proc, output_edit_subclass_id, 0U);
}

bool handle_edit_command(HWND window, int command_id) noexcept {
    HWND edit = active_edit(window);
    switch (command_id) {
        case ID_EDIT_UNDO:
            SendMessageW(edit, EM_UNDO, 0, 0);
            return true;
        case ID_EDIT_CUT:
            SendMessageW(edit, WM_CUT, 0, 0);
            return true;
        case ID_EDIT_COPY:
            SendMessageW(edit, WM_COPY, 0, 0);
            return true;
        case ID_EDIT_PASTE:
            SendMessageW(edit, WM_PASTE, 0, 0);
            return true;
        case ID_EDIT_DELETE:
            SendMessageW(edit, WM_CLEAR, 0, 0);
            return true;
        case ID_EDIT_SELECTALL:
            SendMessageW(edit, EM_SETSEL, 0, -1);
            return true;
        default:
            return false;
    }
}

} // namespace omni::gui::win32
