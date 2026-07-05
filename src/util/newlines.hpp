#pragma once

#include <string>
#include <string_view>

namespace omni::newlines {

// Converts CRLF and standalone CR input into LF. This is the portable form
// used by parsers and writers throughout the shared core.
[[nodiscard]] std::string to_lf(std::string_view text);
[[nodiscard]] std::wstring to_lf(std::wstring_view text);

// Converts LF, CR, and CRLF input into the CRLF sequence required by the
// classic Win32 multiline EDIT control.
[[nodiscard]] std::string to_crlf(std::string_view text);
[[nodiscard]] std::wstring to_crlf(std::wstring_view text);

} // namespace omni::newlines
