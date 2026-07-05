#include "translate/transpose_mode.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace omni::translate {
namespace {

std::string normalize(std::string_view text) {
    std::string value(text);
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [](unsigned char c) { return !std::isspace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [](unsigned char c) { return !std::isspace(c); }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

} // namespace

std::optional<TransposeMode> parse_transpose_mode(std::string_view text) {
    const std::string value = normalize(text);
    if (value == "STRICT" || value == "STRICTED") {
        return TransposeMode::strict;
    }
    if (value == "ORIGINAL" || value == "LEGACY" || value == "CS") {
        return TransposeMode::original;
    }
    return std::nullopt;
}

std::string_view transpose_mode_token(TransposeMode mode) noexcept {
    switch (mode) {
        case TransposeMode::strict: return "STRICT";
        case TransposeMode::original: return "ORIGINAL";
    }
    return "STRICT";
}

std::string_view transpose_mode_name(TransposeMode mode) noexcept {
    switch (mode) {
        case TransposeMode::strict: return "Strict";
        case TransposeMode::original: return "Original";
    }
    return "Strict";
}

} // namespace omni::translate
