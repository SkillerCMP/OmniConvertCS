#pragma once

#include "core/cheat.hpp"
#include "core/crypt.hpp"
#include "translate/translation_error.hpp"
#include "translate/translator.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace omni::translate::detail {

inline constexpr std::uint8_t std_write_byte = 0x0;
inline constexpr std::uint8_t std_write_half = 0x1;
inline constexpr std::uint8_t std_write_word = 0x2;
inline constexpr std::uint8_t std_increment = 0x3;
inline constexpr std::uint8_t std_serial_word = 0x4;
inline constexpr std::uint8_t std_copy_bytes = 0x5;
inline constexpr std::uint8_t std_pointer = 0x6;
inline constexpr std::uint8_t std_bitwise = 0x7;
inline constexpr std::uint8_t std_master_test = 0x8;
inline constexpr std::uint8_t std_cond_hook = 0x9;
inline constexpr std::uint8_t std_once_write = 0xA;
inline constexpr std::uint8_t std_timer = 0xB;
inline constexpr std::uint8_t std_test_all = 0xC;
inline constexpr std::uint8_t std_test_single = 0xD;
inline constexpr std::uint8_t std_test_multi = 0xE;
inline constexpr std::uint8_t std_uncond_hook = 0xF;

inline constexpr std::uint8_t size_byte = 0;
inline constexpr std::uint8_t size_half = 1;
inline constexpr std::uint8_t size_word = 2;
inline constexpr std::uint8_t size_special = 3;

inline constexpr std::uint8_t arm_write = 0;
inline constexpr std::uint8_t arm_equal = 1;
inline constexpr std::uint8_t arm_not_equal = 2;
inline constexpr std::uint8_t arm_less_signed = 3;
inline constexpr std::uint8_t arm_greater_signed = 4;
inline constexpr std::uint8_t arm_less_unsigned = 5;
inline constexpr std::uint8_t arm_greater_unsigned = 6;
inline constexpr std::uint8_t arm_and = 7;

inline constexpr std::uint8_t arm_direct = 0;
inline constexpr std::uint8_t arm_pointer = 1;
inline constexpr std::uint8_t arm_increment = 2;
inline constexpr std::uint8_t arm_hook = 3;

inline constexpr std::uint8_t arm_skip_1 = 0;
inline constexpr std::uint8_t arm_skip_2 = 1;
inline constexpr std::uint8_t arm_skip_n = 2;
inline constexpr std::uint8_t arm_retry = 3;

inline constexpr std::uint32_t arm_special = 0x00000000U;
inline constexpr std::uint32_t arm_resume = 0x40000000U;
inline constexpr std::uint32_t arm_enable_std = 0x0003FF00U;
inline constexpr std::uint32_t arm_enable_int = 0x0002FF01U;
inline constexpr std::uint32_t address_mask = 0x01FFFFFFU;

inline constexpr std::array<std::uint32_t, 3> value_masks{{0xFFU, 0xFFFFU, 0xFFFFFFFFU}};
inline constexpr std::array<std::uint8_t, 7> ar_increment_sizes{{
    size_byte, size_byte, size_byte, size_half, size_half, size_word, size_word}};
inline constexpr std::array<std::uint8_t, 7> standard_increment_sizes{{
    size_byte, size_byte, size_half, size_half, size_word, size_word, size_word}};

[[nodiscard]] constexpr std::uint8_t std_command(std::uint32_t word) noexcept {
    return static_cast<std::uint8_t>((word >> 28U) & 0xFU);
}
[[nodiscard]] constexpr std::uint32_t make_std(std::uint8_t command) noexcept {
    return static_cast<std::uint32_t>(command) << 28U;
}
[[nodiscard]] constexpr std::uint32_t address(std::uint32_t word) noexcept {
    return word & address_mask;
}
[[nodiscard]] constexpr std::uint16_t low_half(std::uint32_t word) noexcept {
    return static_cast<std::uint16_t>(word & 0xFFFFU);
}
[[nodiscard]] constexpr std::uint16_t high_half(std::uint32_t word) noexcept {
    return static_cast<std::uint16_t>((word >> 16U) & 0xFFFFU);
}
[[nodiscard]] constexpr std::uint8_t high_byte(std::uint32_t word) noexcept {
    return static_cast<std::uint8_t>(word >> 24U);
}
[[nodiscard]] constexpr std::uint8_t low_byte(std::uint32_t word) noexcept {
    return static_cast<std::uint8_t>(word & 0xFFU);
}

[[nodiscard]] constexpr std::uint32_t make_arm(std::uint8_t type,
                                                std::uint8_t subtype,
                                                std::uint8_t size) noexcept {
    return (static_cast<std::uint32_t>(subtype) << 30U) |
           (static_cast<std::uint32_t>(type) << 27U) |
           (static_cast<std::uint32_t>(size) << 25U);
}
[[nodiscard]] constexpr std::uint8_t arm_type(std::uint32_t word) noexcept {
    return static_cast<std::uint8_t>((word >> 27U) & 7U);
}
[[nodiscard]] constexpr std::uint8_t arm_subtype(std::uint32_t word) noexcept {
    return static_cast<std::uint8_t>((word >> 30U) & 3U);
}
[[nodiscard]] constexpr std::uint8_t arm_size(std::uint32_t word) noexcept {
    return static_cast<std::uint8_t>((word >> 25U) & 3U);
}
[[nodiscard]] constexpr std::uint8_t arm_mode(std::uint32_t word) noexcept {
    return arm_subtype(word);
}

[[nodiscard]] bool is_ar_family(Device device) noexcept;
[[nodiscard]] bool is_cb(Device device) noexcept;
[[nodiscard]] bool is_gs(Device device) noexcept;
[[nodiscard]] bool is_standard_family(Device device) noexcept;

void require_words(const std::vector<std::uint32_t>& words, std::size_t index,
                   std::size_t count);
void append_pair(Cheat& destination, std::uint32_t first, std::uint32_t second);
void append_warning(Report* report, std::string message);

void translate_standard_to_standard(Cheat& destination, const Cheat& source,
                                    std::size_t& index, Device input,
                                    Device output, Report* report);
void translate_standard_to_armax(Cheat& destination, const Cheat& source,
                                 std::size_t& index, Device input,
                                 Report* report);
void translate_armax_to_standard(Cheat& destination, const Cheat& source,
                                 std::size_t& index, Device output,
                                 Report* report);

void translate_original_standard_to_standard(
    Cheat& destination, const Cheat& source, std::size_t& index, Device input,
    Device output, Report* report);
void translate_original_standard_to_armax(
    Cheat& destination, const Cheat& source, std::size_t& index, Device input,
    Report* report);
void translate_original_armax_to_standard(
    Cheat& destination, const Cheat& source, std::size_t& index, Device output,
    Report* report);

void emit_smash(Cheat& destination, std::uint8_t size, std::uint32_t address_value,
                std::uint32_t value, std::uint32_t count);
void emit_exploded(Cheat& destination, std::uint8_t size,
                   std::uint32_t address_value, std::uint32_t value,
                   std::uint32_t count, std::int32_t byte_step,
                   std::int32_t value_increment);

} // namespace omni::translate::detail
