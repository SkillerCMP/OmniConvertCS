#pragma once

#include "translate/translation_internal.hpp"

#include <cstdint>

namespace omni::translate::detail {

[[nodiscard]] constexpr std::uint32_t original_make_word(
    std::uint32_t value, std::uint8_t size) noexcept {
    return size == size_byte
        ? (value << 24U) | (value << 16U) | (value << 8U) | value
        : (value << 16U) | value;
}

[[nodiscard]] constexpr std::uint32_t original_make_half(
    std::uint32_t value, std::uint8_t size) noexcept {
    return size == size_byte
        ? ((value << 8U) | value) & 0xFFFFU
        : value & 0xFFFFU;
}

void original_smash(Cheat& destination, std::uint8_t size,
                    std::uint32_t address_value, std::uint32_t value,
                    std::uint32_t fill_count);
void original_explode(Cheat& destination, std::uint8_t size,
                      std::uint32_t address_value, std::uint32_t value,
                      std::uint32_t fill_count, std::uint8_t skip,
                      std::uint8_t increment);

} // namespace omni::translate::detail
