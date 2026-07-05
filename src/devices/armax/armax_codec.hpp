#pragma once

#include "core/cheat.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace omni::devices::armax {

inline constexpr std::string_view alphabet = "0123456789ABCDEFGHJKMNPQRTUVWXYZ";
inline constexpr std::size_t encrypted_line_length = 15U;

[[nodiscard]] bool is_encrypted_line(std::string_view text) noexcept;
[[nodiscard]] std::optional<CodePair> decode_line(std::string_view text,
                                                  bool* parity_valid = nullptr) noexcept;
[[nodiscard]] std::string encode_line(const CodePair& pair);

} // namespace omni::devices::armax
