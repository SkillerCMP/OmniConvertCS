#include "translate/translation_internal.hpp"

#include "translate/original_translation_helpers.hpp"

namespace omni::translate::detail {
namespace {

std::uint8_t original_standard_comparison(std::uint8_t type) noexcept {
    if (type == arm_and) return static_cast<std::uint8_t>(type - 2U);
    if (type <= arm_not_equal) return static_cast<std::uint8_t>(type - 1U);
    return static_cast<std::uint8_t>(type - 3U);
}

} // namespace

void translate_original_armax_to_standard(
    Cheat& destination, const Cheat& source, std::size_t& index, Device output,
    Report* report) {
    (void)report;
    require_words(source.words, index, 2U);
    const auto& code = source.words;

    if (code[index] == arm_special) {
        if (code[index + 1U] == arm_resume) {
            index += 2U;
            return;
        }

        require_words(code, index, 4U);
        const std::uint8_t special_type = high_byte(code[index + 1U]);
        if (special_type >= 0x84U && special_type <= 0x85U) {
            append_pair(destination,
                        make_std(std_serial_word) |
                            address(code[index + 1U]),
                        (static_cast<std::uint32_t>(
                             low_byte(high_half(code[index + 3U])))
                         << 16U) |
                            low_half(code[index + 3U]));
            append_pair(destination, code[index + 2U],
                        high_byte(code[index + 3U]));
            index += 4U;
            return;
        }

        if (special_type >= 0x80U && special_type <= 0x83U) {
            original_explode(
                destination, arm_size(code[index + 1U]),
                address(code[index + 1U]), code[index + 2U],
                high_half(code[index + 3U]),
                static_cast<std::uint8_t>(low_half(code[index + 3U])),
                high_byte(code[index + 3U]));
            index += 4U;
            return;
        }

        index += 2U;
        throw TranslationError(ErrorCode::serial_size, index - 2U,
                               "OmniConvertCS multi-address write size error");
    }

    const std::uint32_t first = code[index];
    const std::uint32_t second = code[index + 1U];
    std::uint8_t type = arm_type(first);
    const std::uint8_t subtype = arm_subtype(first);
    const std::uint8_t size = arm_size(first);

    if (type == arm_write) {
        switch (subtype) {
            case arm_direct: {
                std::uint32_t count = 0U;
                std::uint32_t value = 0U;
                if (size == size_byte) {
                    count = second >> 8U;
                    value = low_byte(second);
                } else if (size == size_half) {
                    count = high_half(second);
                    value = low_half(second);
                } else {
                    count = 0U;
                    value = second;
                }

                if (count > 0U) {
                    original_smash(destination, size, address(first), value,
                                   count);
                } else {
                    append_pair(destination,
                                make_std(size) | address(first), value);
                }
                index += 2U;
                return;
            }

            case arm_pointer: {
                std::uint32_t offset = 0U;
                std::uint32_t value = 0U;
                if (size == size_byte) {
                    if (output == Device::gameshark3) {
                        throw TranslationError(ErrorCode::pointer_size, index);
                    }
                    offset = second >> 8U;
                    value = low_byte(second);
                } else if (size == size_half) {
                    offset = high_half(second);
                    value = low_half(second);
                } else {
                    offset = 0U;
                    value = second;
                }

                if (output == Device::codebreaker) {
                    append_pair(destination,
                                make_std(std_pointer) | address(first), value);
                    append_pair(destination,
                                (static_cast<std::uint32_t>(size) << 16U) |
                                    1U,
                                offset);
                } else {
                    append_pair(destination,
                                make_std(std_pointer) |
                                    (first & 0x00FFFFFFU),
                                first & 0x01000000U);
                    append_pair(destination, offset, value);
                }
                index += 2U;
                return;
            }

            case arm_increment: {
                const std::uint32_t raw_subtype =
                    (output == Device::ar1 || output == Device::ar2)
                    ? (static_cast<std::uint32_t>(size) << 1U) + 1U
                    : (static_cast<std::uint32_t>(size) << 1U);

                append_pair(destination,
                            size < size_word
                                ? make_std(std_increment) |
                                      (raw_subtype << 20U) |
                                      (second & value_masks[size])
                                : make_std(std_increment) |
                                      (raw_subtype << 20U),
                            address(first));
                index += 2U;

                // Preserve the exact OmniConvertCS condition (> word rather
                // than >= word).
                if (size > size_word) {
                    require_words(code, index, 2U);
                    destination.append_word(code[index + 1U]);
                    index += 2U;
                }
                return;
            }

            case arm_hook:
                if (index >= 2U &&
                    arm_type(code[index - 2U]) == arm_equal &&
                    arm_subtype(code[index - 2U]) == arm_skip_1 &&
                    address(code[index - 2U]) == address(first) &&
                    destination.words.size() >= 2U) {
                    destination.words[destination.words.size() - 2U] =
                        make_std(std_cond_hook) | address(first);
                } else {
                    append_pair(destination,
                                make_std(std_uncond_hook) | address(first),
                                address(first) + 3U);
                }
                index += 2U;
                return;
        }
    }

    // OmniConvertCS deliberately treats signed comparisons as unsigned.
    if (type == arm_less_signed) type = arm_less_unsigned;
    else if (type == arm_greater_signed) type = arm_greater_unsigned;

    if (type == arm_and && output != Device::codebreaker) {
        throw TranslationError(ErrorCode::comparison, index);
    }
    if (size == size_byte && output != Device::codebreaker) {
        throw TranslationError(ErrorCode::test_size, index);
    }
    if (size >= value_masks.size()) {
        throw TranslationError(ErrorCode::test_size, index);
    }

    bool translated = false;
    if (size == size_word) {
        bool conditional_hook = false;
        if (type == arm_equal && index + 2U < code.size()) {
            const std::uint8_t next_type = arm_type(code[index + 2U]);
            const std::uint8_t next_subtype =
                arm_subtype(code[index + 2U]);
            if (next_type == arm_write && next_subtype == arm_hook) {
                if (output == Device::ar2) {
                    append_pair(destination,
                                make_std(std_master_test) |
                                    (address(first) + 1U),
                                address(first));
                    append_pair(destination, address(second), 0U);
                } else {
                    append_pair(destination,
                                make_std(std_cond_hook) | address(first),
                                second);
                }
                index += 4U;
                translated = true;
                conditional_hook = true;
            }
        }

        if (!conditional_hook) {
            const std::uint32_t address32 = address(first);
            const std::uint32_t value32 = second;
            const std::uint32_t low_value = low_half(value32);
            const std::uint32_t high_value = high_half(value32);
            const std::uint32_t low_address = address32;
            const std::uint32_t high_address = address32 + 2U;
            const std::uint8_t standard_type =
                original_standard_comparison(type);

            std::size_t body_end = 0U;
            if (subtype == arm_skip_1) body_end = index + 4U;
            else if (subtype == arm_skip_2) body_end = index + 6U;
            else body_end = code.size();

            const std::size_t outer_index = destination.words.size();
            append_pair(destination, 0U,
                        low_address |
                            (static_cast<std::uint32_t>(standard_type)
                             << 28U));
            const std::size_t inner_index = destination.words.size();
            append_pair(destination, 0U,
                        high_address |
                            (static_cast<std::uint32_t>(standard_type)
                             << 28U));

            index += 2U;
            const std::size_t body_start = destination.words.size();
            while (index < code.size() && index < body_end) {
                if (index + 1U < code.size() &&
                    code[index] == arm_special &&
                    code[index + 1U] == arm_resume) {
                    index += 2U;
                    break;
                }
                translate_original_armax_to_standard(
                    destination, source, index, output, report);
            }

            const std::uint32_t body_lines = static_cast<std::uint32_t>(
                (destination.words.size() - body_start) >> 1U);
            const std::uint32_t outer_lines = body_lines + 1U;
            const std::uint32_t inner_lines = body_lines;

            destination.words[outer_index] =
                make_std(std_test_multi) | (outer_lines << 16U) |
                low_value;
            destination.words[inner_index] =
                make_std(std_test_multi) | (inner_lines << 16U) |
                high_value;
            translated = true;
        }
    }

    if (!translated) {
        const std::uint32_t compare_address = address(first);
        const std::uint32_t compare_value = second & value_masks[size];
        const std::uint8_t standard_type =
            original_standard_comparison(type);

        std::size_t skip = 0U;
        if (subtype == arm_skip_1) skip = 2U;
        else if (subtype == arm_skip_2) skip = 4U;
        else skip = code.size();

        const std::size_t output_index = destination.words.size();
        const std::uint8_t standard_size =
            size == size_half ? 0U : 1U;
        append_pair(destination, 0U,
                    compare_address |
                        (static_cast<std::uint32_t>(standard_type) << 28U));
        index += 2U;
        const std::size_t body_start = destination.words.size();
        skip += index;

        while (index < code.size() && index < skip) {
            if (index + 1U < code.size() &&
                code[index] == arm_special &&
                code[index + 1U] == arm_resume) {
                index += 2U;
                break;
            }
            translate_original_armax_to_standard(
                destination, source, index, output, report);
        }

        const std::uint32_t translated_lines =
            static_cast<std::uint32_t>(
                (destination.words.size() - body_start) >> 1U);
        const bool force_multi = output == Device::standard ||
                                 output == Device::gameshark3 ||
                                 output == Device::codebreaker;
        if (force_multi || translated_lines != 1U) {
            destination.words[output_index] =
                make_std(std_test_multi) |
                (static_cast<std::uint32_t>(standard_size) << 24U) |
                (translated_lines << 16U) | compare_value;
        } else {
            destination.words[output_index] =
                make_std(std_test_single) | compare_address;
            destination.words[output_index + 1U] =
                (static_cast<std::uint32_t>(standard_type) << 20U) |
                (static_cast<std::uint32_t>(standard_size) << 16U) |
                compare_value;
        }
    }
}

} // namespace omni::translate::detail
