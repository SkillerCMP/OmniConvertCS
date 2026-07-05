#include "gui/win32/text_control.hpp"

#include "util/newlines.hpp"

#include <stdexcept>
#include <vector>

namespace omni::gui::win32 {
namespace {

std::wstring convert_to_wide(std::string_view text, UINT code_page, DWORD flags) {
    if (text.empty()) return {};
    const int required = MultiByteToWideChar(
        code_page, flags, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0) throw std::runtime_error("Unable to convert text to UTF-16");

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(code_page, flags, text.data(), static_cast<int>(text.size()),
                            result.data(), required) <= 0) {
        throw std::runtime_error("Unable to convert text to UTF-16");
    }
    return result;
}

} // namespace

std::wstring utf8_to_utf16(std::string_view text) {
    try {
        return convert_to_wide(text, CP_UTF8, MB_ERR_INVALID_CHARS);
    } catch (const std::runtime_error&) {
        return convert_to_wide(text, CP_ACP, 0);
    }
}

std::string utf16_to_utf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) throw std::runtime_error("Unable to convert text to UTF-8");

    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), result.data(), required,
                            nullptr, nullptr) <= 0) {
        throw std::runtime_error("Unable to convert text to UTF-8");
    }
    return result;
}

std::string read_window_text_utf8(HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};

    std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1U, L'\0');
    const int copied = GetWindowTextW(control, buffer.data(), static_cast<int>(buffer.size()));
    if (copied < 0) throw std::runtime_error("Unable to read text control");
    return utf16_to_utf8(std::wstring_view(buffer.data(), static_cast<std::size_t>(copied)));
}

void set_window_text_utf8(HWND control, std::string_view text) {
    // The classic Win32 multiline EDIT control expects CRLF. The portable
    // conversion core intentionally emits LF, so normalize only when text is
    // placed into a Windows edit control.
    const std::string windows_text = newlines::to_crlf(text);
    const std::wstring wide = utf8_to_utf16(windows_text);
    SetWindowTextW(control, wide.c_str());
}

} // namespace omni::gui::win32
