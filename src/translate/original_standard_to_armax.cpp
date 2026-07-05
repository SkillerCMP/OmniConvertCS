#include "translate/translation_internal.hpp"

#include <array>

namespace omni::translate::detail {
namespace {

inline constexpr std::array<std::uint8_t, 6> original_std_compare_to_arm{{
    arm_equal, arm_not_equal, arm_less_unsigned, arm_greater_unsigned,
    arm_and, arm_and}};

} // namespace

void translate_original_standard_to_armax(
    Cheat& destination, const Cheat& source, std::size_t& index, Device input,
    Report* report) {
    (void)report;
    require_words(source.words, index, 2U);

    const auto& code = source.words;
    const std::uint8_t command = std_command(code[index]);

    switch (command) {
        case std_write_byte:
        case std_write_half:
        case std_write_word:
            append_pair(destination,
                        make_arm(arm_write, arm_direct, command) |
                            address(code[index]),
                        code[index + 1U]);
            index += 2U;
            return;

        case std_increment: {
            const std::uint8_t raw = static_cast<std::uint8_t>(
                (code[index] >> 20U) & 7U);
            if (raw >= ar_increment_sizes.size()) {
                throw TranslationError(ErrorCode::invalid_code, index);
            }

            std::uint8_t size = 0U;
            std::uint32_t decrement = 0U;
            if (input == Device::ar1 || input == Device::ar2) {
                size = ar_increment_sizes[raw];
                decrement = (raw & 1U) != 0U ? 0U : 1U;
            } else {
                size = standard_increment_sizes[raw];
                decrement = (raw & 1U) != 0U ? 1U : 0U;
            }

            std::uint32_t value = 0U;
            std::size_t consumed = 2U;
            if (size == size_word) {
                // OmniConvertCS uses this exact >= check.
                if (index + 4U >= code.size()) {
                    throw TranslationError(ErrorCode::invalid_code, index);
                }
                value = code[index + 2U];
                consumed = 4U;
            } else {
                value = low_half(code[index]);
            }

            append_pair(destination,
                        make_arm(arm_write, arm_increment, size) |
                            address(code[index + 1U]),
                        decrement != 0U ? (~value) + 1U : value);
            index += consumed;
            return;
        }

        case std_serial_word: {
            require_words(code, index, 4U);
            const std::uint32_t increment = code[index + 3U];
            if (increment > 127U) {
                throw TranslationError(ErrorCode::value_increment, index);
            }
            append_pair(destination, arm_special,
                        make_arm(arm_write, arm_increment, size_word) |
                            address(code[index]));
            append_pair(destination, code[index + 2U],
                        ((increment & 0xFFU) << 24U) |
                            (static_cast<std::uint32_t>(
                                 high_half(code[index + 1U]))
                             << 16U) |
                            low_half(code[index + 1U]));
            index += 4U;
            return;
        }

        case std_copy_bytes:
            require_words(code, index, 4U);
            index += 4U;
            throw TranslationError(ErrorCode::copy_bytes, index - 4U);

        case std_pointer: {
            require_words(code, index, 4U);
            std::uint8_t size = 0U;
            std::uint32_t base_address = 0U;
            std::uint32_t value = 0U;
            std::uint32_t count = 0U;
            std::uint32_t offset = 0U;

            if (input == Device::codebreaker) {
                size = static_cast<std::uint8_t>(
                    high_half(code[index + 2U]) & 3U);
                base_address = address(code[index]);
                if (size >= value_masks.size()) {
                    throw TranslationError(ErrorCode::pointer_size, index);
                }
                value = code[index + 1U] & value_masks[size];
                count = low_half(code[index + 2U]);
                offset = code[index + 3U];
            } else {
                size = static_cast<std::uint8_t>(high_byte(code[index]) & 0xFU);
                if (size < 1U && input == Device::gameshark3) size = 1U;
                if (size >= value_masks.size()) {
                    throw TranslationError(ErrorCode::pointer_size, index);
                }
                base_address = (code[index] & 0x00FFFFFFU) +
                               code[index + 1U];
                offset = code[index + 2U];
                value = code[index + 3U] & value_masks[size];
                count = 0U;
            }
            const std::size_t header_index = index;
            index += 4U;

            if (count > 1U) {
                index += static_cast<std::size_t>((count >> 1U) << 1U);
                throw TranslationError(ErrorCode::excess_offsets,
                                       header_index);
            }
            if (size == size_word && offset > 0U) {
                throw TranslationError(ErrorCode::offset_value_32,
                                       header_index);
            }
            if (size == size_half && offset > 0xFFFFU) {
                throw TranslationError(ErrorCode::offset_value_16,
                                       header_index);
            }
            if (size == size_byte && offset > 0xFFFFFFU) {
                throw TranslationError(ErrorCode::offset_value_8,
                                       header_index);
            }

            append_pair(destination,
                        make_arm(arm_write, arm_pointer, size) |
                            base_address,
                        (offset << 16U) | value);
            return;
        }

        case std_bitwise:
            index += 2U;
            throw TranslationError(ErrorCode::bitwise, index - 2U);

        case std_master_test:
            if (input == Device::ar1 || input == Device::ar2) {
                require_words(code, index, 4U);
                append_pair(destination,
                            make_arm(arm_equal, arm_skip_1, size_word) |
                                address(code[index + 1U]),
                            code[index + 2U]);
                append_pair(destination,
                            make_arm(arm_write, arm_hook, size_word) |
                                address(code[index + 1U]),
                            arm_enable_std);
                index += 4U;
                return;
            } else {
                append_pair(destination,
                            make_arm(arm_equal, arm_skip_1, size_half) |
                                address(code[index]),
                            low_half(code[index + 1U]));
                const std::size_t held_word = destination.words.size() - 2U;
                std::uint32_t skip =
                    static_cast<std::uint32_t>(high_half(code[index + 1U]))
                    << 1U;
                index += 2U;
                skip += static_cast<std::uint32_t>(index);
                std::uint32_t lines = 0U;
                while (index < code.size() && index < skip) {
                    translate_original_standard_to_armax(
                        destination, source, index, input, report);
                    ++lines;
                }
                if (lines > 1U) {
                    if (lines == 2U) {
                        destination.words[held_word] =
                            make_arm(arm_equal, arm_skip_2, size_half) |
                            address(destination.words[held_word]);
                    } else {
                        destination.words[held_word] =
                            make_arm(arm_equal, arm_skip_n, size_half) |
                            address(destination.words[held_word]);
                        append_pair(destination, arm_special, arm_resume);
                    }
                }
                return;
            }

        case std_cond_hook:
            append_pair(destination,
                        make_arm(arm_equal, arm_skip_1, size_word) |
                            address(code[index]),
                        code[index + 1U]);
            append_pair(destination,
                        make_arm(arm_write, arm_hook, size_word) |
                            address(code[index]),
                        arm_enable_std);
            index += 2U;
            return;

        case std_once_write:
            append_pair(destination,
                        make_arm(arm_write, arm_direct, size_word) |
                            address(code[index]),
                        code[index + 1U]);
            index += 2U;
            return;

        case std_timer:
            index += 2U;
            throw TranslationError(ErrorCode::timer, index - 2U);

        case std_test_all:
            index += 2U;
            throw TranslationError(ErrorCode::all_off, index - 2U);

        case std_test_single: {
            const std::uint8_t comparison = static_cast<std::uint8_t>(
                (code[index + 1U] >> 20U) & 7U);
            if (comparison > 5U || comparison == 4U) {
                throw TranslationError(ErrorCode::comparison, index);
            }
            const std::uint8_t size = input == Device::codebreaker
                ? static_cast<std::uint8_t>(
                      (high_half(code[index + 1U]) & 1U) ^ 1U)
                : size_half;
            append_pair(destination,
                        make_arm(original_std_compare_to_arm[comparison],
                                 arm_skip_1, size) |
                            address(code[index]),
                        low_half(code[index + 1U]));
            index += 2U;
            return;
        }

        case std_test_multi: {
            const std::uint8_t comparison = static_cast<std::uint8_t>(
                (code[index + 1U] >> 28U) & 7U);
            if (comparison > 5U || comparison == 4U) {
                throw TranslationError(ErrorCode::comparison, index);
            }
            const std::uint8_t size = input == Device::codebreaker
                ? static_cast<std::uint8_t>(
                      (high_byte(code[index]) & 1U) ^ 1U)
                : size_half;
            const std::uint32_t skip = input == Device::codebreaker
                ? high_half(code[index]) & 0xFFU
                : high_half(code[index]) & 0xFFFU;
            const std::uint32_t count = skip <= 2U
                ? skip - 1U
                : arm_skip_n;

            append_pair(destination,
                        make_arm(original_std_compare_to_arm[comparison],
                                 static_cast<std::uint8_t>(count), size) |
                            address(code[index + 1U]),
                        low_half(code[index]));
            index += 2U;

            if (skip > 2U) {
                const std::uint32_t end =
                    (skip << 1U) + static_cast<std::uint32_t>(index);
                while (index < code.size() && index < end) {
                    translate_original_standard_to_armax(
                        destination, source, index, input, report);
                }
                append_pair(destination, arm_special, arm_resume);
            }
            return;
        }

        case std_uncond_hook: {
            std::uint32_t hook_address = address(code[index]);
            std::uint32_t enable = arm_enable_std;
            if (hook_address == 0x00100008U ||
                hook_address == 0x00100000U ||
                hook_address == 0x00200000U ||
                hook_address == 0x00200008U) {
                if (code[index + 1U] == 0x000001FDU ||
                    code[index + 1U] == 0x0000000EU) {
                    enable = arm_enable_int;
                } else {
                    hook_address = code[index + 1U] & 0x01FFFFFCU;
                }
            }
            append_pair(destination,
                        make_arm(arm_write, arm_hook, size_word) |
                            hook_address,
                        enable);
            index += 2U;
            return;
        }
    }

    throw TranslationError(ErrorCode::invalid_code, index);
}

} // namespace omni::translate::detail
