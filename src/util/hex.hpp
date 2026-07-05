#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace omni::hex {

[[nodiscard]] bool is_hex_digit(char value) noexcept;
[[nodiscard]] std::optional<std::uint32_t> parse_u32(std::string_view text);
[[nodiscard]] std::string format_u32(std::uint32_t value);
[[nodiscard]] std::string trim(std::string_view text);

} // namespace omni::hex
