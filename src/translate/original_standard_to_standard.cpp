#include "translate/translation_internal.hpp"

namespace omni::translate::detail {

void translate_original_standard_to_standard(
    Cheat& destination, const Cheat& source, std::size_t& index, Device input,
    Device output, Report* report) {
    (void)report;
    require_words(source.words, index, 2U);

    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];
    const std::uint8_t command = std_command(first);

    switch (command) {
        case std_write_byte:
        case std_write_half:
        case std_write_word:
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_increment: {
            std::uint32_t adjusted = first;
            std::uint8_t size = 0U;
            if (input == Device::ar2 || input == Device::ar1) {
                if (output == Device::codebreaker ||
                    output == Device::gameshark3) {
                    adjusted -= 0x00010000U;
                }
                size = static_cast<std::uint8_t>(
                    (((adjusted >> 20U) & 7U) - 1U) & 0xFFU);
            } else {
                size = static_cast<std::uint8_t>((adjusted >> 20U) & 7U);
                if (output == Device::ar2 || output == Device::ar1) {
                    adjusted += 0x00010000U;
                }
            }

            append_pair(destination, adjusted, second);
            index += 2U;
            if (size > 3U) {
                require_words(source.words, index, 2U);
                append_pair(destination, source.words[index],
                            source.words[index + 1U]);
                index += 2U;
            }
            return;
        }

        case std_serial_word:
            require_words(source.words, index, 4U);
            if ((output == Device::ar2 || output == Device::ar1) &&
                source.words[index + 3U] > 0U) {
                throw TranslationError(ErrorCode::value_increment, index);
            }
            append_pair(destination, first, second);
            append_pair(destination, source.words[index + 2U],
                        source.words[index + 3U]);
            index += 4U;
            return;

        case std_copy_bytes:
            require_words(source.words, index, 4U);
            append_pair(destination, first, second);
            append_pair(destination, source.words[index + 2U],
                        source.words[index + 3U]);
            index += 4U;
            return;

        case std_pointer: {
            require_words(source.words, index, 4U);
            if (output == Device::standard || input == Device::standard ||
                (output != Device::codebreaker &&
                 input != Device::codebreaker)) {
                append_pair(destination, first, second);
                append_pair(destination, source.words[index + 2U],
                            source.words[index + 3U]);
                index += 4U;
                return;
            }

            if (input == Device::codebreaker) {
                if (low_half(source.words[index + 2U]) > 1U) {
                    throw TranslationError(ErrorCode::excess_offsets, index);
                }
                const std::uint32_t full_address = address(first);
                const std::uint32_t offset = full_address & 0x01000000U;
                const std::uint8_t size = static_cast<std::uint8_t>(
                    high_half(source.words[index + 2U]));
                if (size == 0U) {
                    throw TranslationError(ErrorCode::pointer_size, index);
                }
                append_pair(destination,
                            make_std(std_pointer) |
                                (static_cast<std::uint32_t>(size) << 24U) |
                                (full_address & 0x00FFFFFFU),
                            offset);
                append_pair(destination, source.words[index + 3U], second);
            } else {
                std::uint8_t size = static_cast<std::uint8_t>(
                    (first >> 24U) & 3U);
                if (size == 0U || size == 3U) size = 1U;
                append_pair(destination,
                            (first & 0xF0FFFFFFU) + second,
                            source.words[index + 3U]);
                append_pair(destination,
                            1U | (static_cast<std::uint32_t>(size) << 16U),
                            source.words[index + 2U]);
            }
            index += 4U;
            return;
        }

        case std_bitwise:
            if (input == Device::codebreaker || input == Device::standard) {
                if (output == Device::codebreaker ||
                    output == Device::standard) {
                    append_pair(destination, first, second);
                } else {
                    throw TranslationError(ErrorCode::bitwise, index);
                }
            }
            index += 2U;
            return;

        case std_master_test:
            if (input == Device::ar2 || input == Device::ar1) {
                require_words(source.words, index, 4U);
                if (output == Device::standard || output == Device::ar1 ||
                    output == Device::ar2) {
                    append_pair(destination, first, second);
                    append_pair(destination, source.words[index + 2U],
                                source.words[index + 3U]);
                } else {
                    append_pair(destination,
                                make_std(std_cond_hook) | address(second),
                                source.words[index + 2U]);
                }
                index += 4U;
                return;
            }

            if (input == Device::codebreaker ||
                input == Device::gameshark3) {
                if (output == Device::standard) {
                    append_pair(destination, first, second);
                    index += 2U;
                    return;
                }
                throw TranslationError(ErrorCode::comparison, index);
            }

            require_words(source.words, index, 4U);
            if (std_command(source.words[index + 2U]) == 0U) {
                // Preserve the duplicated AR2 check from OmniConvertCS.
                if (output == Device::ar2) {
                    append_pair(destination, first, second);
                    append_pair(destination, source.words[index + 2U],
                                source.words[index + 3U]);
                } else {
                    append_pair(destination,
                                make_std(std_cond_hook) | address(second),
                                source.words[index + 2U]);
                }
                index += 4U;
                return;
            }

            if (output == Device::ar2) {
                throw TranslationError(ErrorCode::comparison, index);
            }
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_cond_hook:
            if (output == Device::ar1 || output == Device::ar2) {
                append_pair(destination,
                            make_std(std_master_test) | (address(first) + 1U),
                            address(first));
                append_pair(destination, second, 0U);
            } else {
                append_pair(destination, first, second);
            }
            index += 2U;
            return;

        case std_once_write:
        case std_timer:
        case std_test_all:
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_test_single:
            if (input == Device::codebreaker &&
                output != Device::standard &&
                (((second >> 16U) & 0xFU) > 0U)) {
                throw TranslationError(ErrorCode::test_size, index);
            }
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_test_multi:
            if (input == Device::codebreaker &&
                output != Device::standard &&
                (((first >> 24U) & 0xFU) > 0U)) {
                throw TranslationError(ErrorCode::test_size, index);
            }
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_uncond_hook:
            append_pair(destination, first, second);
            index += 2U;
            return;
    }

    throw TranslationError(ErrorCode::invalid_code, index);
}

} // namespace omni::translate::detail
