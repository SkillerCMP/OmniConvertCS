#pragma once

#include <optional>
#include <string_view>

namespace omni::translate {

enum class TransposeMode {
    strict,
    original
};

[[nodiscard]] std::optional<TransposeMode> parse_transpose_mode(
    std::string_view text);
[[nodiscard]] std::string_view transpose_mode_token(
    TransposeMode mode) noexcept;
[[nodiscard]] std::string_view transpose_mode_name(
    TransposeMode mode) noexcept;

} // namespace omni::translate
