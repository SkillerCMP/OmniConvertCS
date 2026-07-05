#include "translate/translation_internal.hpp"
#include "translate/cb_gs_semantics.hpp"

#include <sstream>

namespace omni::translate::detail {
namespace {

std::uint8_t normalize_comparison(std::uint8_t condition, Device input,
                                  Device output, std::size_t index) {
    if (input == output || input == Device::standard || output == Device::standard) {
        return condition;
    }

    // Equality and inequality are shared. Other condition numbers are not
    // portable: CodeBreaker and GS3 assign different meanings to at least
    // condition 3, while GS3 does not support CB conditions 4-7.
    if (condition <= 1U) return condition;

    throw TranslationError(
        ErrorCode::comparison, index,
        "only equal/not-equal comparisons can be translated safely between these device families");
}

std::uint32_t condition_word_for_output(std::uint32_t source_word,
                                        std::uint8_t condition) noexcept {
    return (source_word & 0xFF0FFFFFU) |
           (static_cast<std::uint32_t>(condition) << 20U);
}

void copy_words(Cheat& destination, const Cheat& source, std::size_t index,
                std::size_t count) {
    require_words(source.words, index, count);
    for (std::size_t offset = 0U; offset < count; ++offset) {
        destination.append_word(source.words[index + offset]);
    }
}

} // namespace

void translate_standard_to_standard(Cheat& destination, const Cheat& source,
                                    std::size_t& index, Device input,
                                    Device output, Report* report) {
    require_words(source.words, index, 2U);
    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];
    const std::uint8_t command = std_command(first);

    if (is_codebreaker_gameshark_pair(input, output)) {
        translate_codebreaker_gameshark(destination, source, index, input,
                                        output, report);
        return;
    }

    switch (command) {
        case std_write_byte:
        case std_write_half:
        case std_write_word:
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_increment: {
            std::uint32_t adjusted = first;
            const bool input_ar = is_ar_family(input);
            const bool output_ar = is_ar_family(output);
            const std::uint8_t raw_subtype = static_cast<std::uint8_t>((first >> 20U) & 7U);

            if (input_ar != output_ar) {
                if (input_ar) {
                    if (raw_subtype == 0U) {
                        throw TranslationError(ErrorCode::invalid_code, index,
                                               "AR increment subtype zero is invalid");
                    }
                    adjusted -= 0x00010000U;
                } else {
                    if (raw_subtype == 7U) {
                        throw TranslationError(ErrorCode::invalid_code, index,
                                               "increment subtype cannot be shifted into AR format");
                    }
                    adjusted += 0x00010000U;
                }
            }

            const std::uint8_t semantic_subtype = input_ar
                ? static_cast<std::uint8_t>(raw_subtype - 1U)
                : raw_subtype;
            append_pair(destination, adjusted, second);
            index += 2U;

            if (semantic_subtype >= 4U) {
                require_words(source.words, index, 2U);
                append_pair(destination, source.words[index], source.words[index + 1U]);
                index += 2U;
            }
            return;
        }

        case std_serial_word:
            require_words(source.words, index, 4U);
            if (is_ar_family(output) && source.words[index + 3U] != 0U) {
                throw TranslationError(ErrorCode::value_increment, index,
                                       "AR1/AR2 serial writes do not preserve a nonzero value increment");
            }
            copy_words(destination, source, index, 4U);
            index += 4U;
            return;

        case std_copy_bytes:
            require_words(source.words, index, 4U);
            copy_words(destination, source, index, 4U);
            index += 4U;
            return;

        case std_pointer: {
            require_words(source.words, index, 4U);

            // Standard RAW is an explicit neutral container. Keep its selected
            // representation unchanged when entering or leaving that format.
            if (input == Device::standard || output == Device::standard ||
                (!is_cb(input) && !is_cb(output))) {
                copy_words(destination, source, index, 4U);
                index += 4U;
                return;
            }

            if (is_cb(input)) {
                const std::uint32_t descriptor = source.words[index + 2U];
                const std::uint16_t count = low_half(descriptor);
                const std::uint8_t size = static_cast<std::uint8_t>(high_half(descriptor) & 3U);
                if (count > 1U) {
                    throw TranslationError(ErrorCode::excess_offsets, index,
                                           "AR/GS pointer format supports one final offset only");
                }
                if (size == size_byte || size == size_special) {
                    throw TranslationError(ErrorCode::pointer_size, index,
                                           "AR/GS pointer format has no exact 8-bit mapping");
                }

                const std::uint32_t full_address = address(first);
                append_pair(destination,
                            make_std(std_pointer) |
                                (static_cast<std::uint32_t>(size) << 24U) |
                                (full_address & 0x00FFFFFFU),
                            full_address & 0x01000000U);
                append_pair(destination, source.words[index + 3U], second);
            } else {
                std::uint8_t size = static_cast<std::uint8_t>((first >> 24U) & 3U);
                // GS/AR type 60 and 61 are 16-bit aliases; 62 is 32-bit.
                if (size == 0U || size == 3U) size = size_half;

                const std::uint32_t full_address =
                    (first & 0x00FFFFFFU) + second;
                append_pair(destination, make_std(std_pointer) | address(full_address),
                            source.words[index + 3U]);
                append_pair(destination,
                            1U | (static_cast<std::uint32_t>(size) << 16U),
                            source.words[index + 2U]);
            }
            index += 4U;
            return;
        }

        case std_bitwise:
            if ((is_cb(input) || input == Device::standard) &&
                (is_cb(output) || output == Device::standard)) {
                append_pair(destination, first, second);
                index += 2U;
                return;
            }
            throw TranslationError(ErrorCode::bitwise, index);

        case std_master_test:
            if (is_ar_family(input)) {
                require_words(source.words, index, 4U);
                if (is_ar_family(output) || output == Device::standard) {
                    copy_words(destination, source, index, 4U);
                } else {
                    append_pair(destination,
                                make_std(std_cond_hook) | address(source.words[index + 1U]),
                                source.words[index + 2U]);
                }
                index += 4U;
                return;
            }

            if (is_cb(input) || is_gs(input)) {
                if (is_cb(output) || is_gs(output) || output == Device::standard) {
                    // Type 8 is accepted by both CB and GS setup paths. The old
                    // translator rejected this CB<->GS case unnecessarily.
                    append_pair(destination, first, second);
                    index += 2U;
                    return;
                }
                throw TranslationError(ErrorCode::semantic_loss, index,
                                       "setup-time type-8 gates cannot be represented by AR1/AR2 hook records");
            }

            // Neutral RAW can contain either the four-word AR hook form or the
            // two-word CB/GS setup gate. Detect the AR form by its continuation.
            if (source.words.size() - index >= 4U &&
                std_command(source.words[index + 2U]) == std_write_byte) {
                if (is_ar_family(output)) {
                    copy_words(destination, source, index, 4U);
                } else {
                    append_pair(destination,
                                make_std(std_cond_hook) | address(source.words[index + 1U]),
                                source.words[index + 2U]);
                }
                index += 4U;
            } else {
                if (is_ar_family(output)) {
                    throw TranslationError(ErrorCode::semantic_loss, index,
                                           "neutral type-8 setup gate is not an AR conditional hook");
                }
                append_pair(destination, first, second);
                index += 2U;
            }
            return;

        case std_cond_hook:
            if (is_ar_family(output)) {
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
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_timer: {
            if (is_cb(input) && is_gs(output)) {
                const std::uint8_t subtype = static_cast<std::uint8_t>(first & 0xFU);
                if (subtype != 0U) {
                    throw TranslationError(ErrorCode::unsupported_control, index,
                                           "CodeBreaker B subtype is not the delay subtype");
                }
                append_pair(destination, make_std(std_timer), second);
            } else if (is_gs(input) && is_cb(output)) {
                // GS ignores the first word fields and behaves as B0 delay.
                append_pair(destination, make_std(std_timer), second);
            } else {
                append_pair(destination, first, second);
            }
            index += 2U;
            return;
        }

        case std_test_all:
            if ((is_cb(input) && is_gs(output)) ||
                (is_gs(input) && is_cb(output))) {
                throw TranslationError(ErrorCode::semantic_loss, index,
                                       "CB C is a persistent gate while GS C is a one-shot wait gate");
            }
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_test_single: {
            std::uint8_t condition = static_cast<std::uint8_t>((second >> 20U) & 7U);
            const std::uint8_t width = static_cast<std::uint8_t>((second >> 16U) & 0xFU);
            if (is_cb(input) && !is_cb(output) && output != Device::standard && width != 0U) {
                throw TranslationError(ErrorCode::test_size, index,
                                       "8-bit CB D conditions have no exact GS/AR mapping");
            }
            condition = normalize_comparison(condition, input, output, index);
            append_pair(destination, first, condition_word_for_output(second, condition));
            index += 2U;
            return;
        }

        case std_test_multi: {
            const bool input_cb = is_cb(input);
            const bool output_cb = is_cb(output);
            const std::uint8_t input_width = input_cb
                ? static_cast<std::uint8_t>((first >> 24U) & 1U)
                : 0U;
            const std::uint32_t input_count = input_cb
                ? ((first >> 16U) & 0xFFU)
                : ((first >> 16U) & 0xFFFU);
            std::uint8_t condition = static_cast<std::uint8_t>((second >> 28U) & 7U);

            if (input_cb && !output_cb && output != Device::standard && input_width != 0U) {
                throw TranslationError(ErrorCode::test_size, index,
                                       "8-bit CB E conditions have no exact GS/AR mapping");
            }
            condition = normalize_comparison(condition, input, output, index);

            if (output == Device::standard || input == Device::standard) {
                append_pair(destination, first,
                            (second & 0x0FFFFFFFU) |
                                (static_cast<std::uint32_t>(condition) << 28U));
            } else if (output_cb) {
                if (input_count > 0xFFU) {
                    throw TranslationError(ErrorCode::count_overflow, index,
                                           "CodeBreaker E has an 8-bit line-count field");
                }
                const std::uint32_t converted_first =
                    make_std(std_test_multi) |
                    (static_cast<std::uint32_t>(input_width) << 24U) |
                    (input_count << 16U) |
                    low_half(first);
                append_pair(destination, converted_first,
                            (second & 0x0FFFFFFFU) |
                                (static_cast<std::uint32_t>(condition) << 28U));
            } else {
                if (input_count > 0xFFFU) {
                    throw TranslationError(ErrorCode::count_overflow, index,
                                           "GS/AR E has a 12-bit line-count field");
                }
                const std::uint32_t converted_first =
                    make_std(std_test_multi) | (input_count << 16U) | low_half(first);
                append_pair(destination, converted_first,
                            (second & 0x0FFFFFFFU) |
                                (static_cast<std::uint32_t>(condition) << 28U));
            }
            index += 2U;
            return;
        }

        case std_uncond_hook:
            // AR1/AR2, CodeBreaker, and GS3 all expose the standard two-word
            // type-F hook record in their public RAW formats. Preserve the
            // complete pair, including the low two mode bits in the second
            // word. Device-specific exceptions between CodeBreaker and GS3
            // are handled earlier by the dedicated semantic translator.
            append_pair(destination, first, second);
            index += 2U;
            return;
    }

    throw TranslationError(ErrorCode::invalid_code, index,
                           "unknown standard command type");
}

} // namespace omni::translate::detail
