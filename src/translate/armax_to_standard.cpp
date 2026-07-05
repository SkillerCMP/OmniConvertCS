#include "translate/translation_internal.hpp"

#include "translate/armax_conditions.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace omni::translate::detail {
namespace {

std::int32_t sign_extend_24(std::uint32_t value) noexcept {
    value &= 0x00FFFFFFU;
    if ((value & 0x00800000U) != 0U) {
        value |= 0xFF000000U;
    }
    return static_cast<std::int32_t>(value);
}

void append_words(Cheat& destination, const Cheat& source) {
    for (const std::uint32_t word : source.words) {
        destination.append_word(word);
    }
}

[[nodiscard]] bool uses_cb_condition_layout(Device output) noexcept {
    return is_cb(output) || output == Device::standard;
}

std::size_t standard_command_count(const Cheat& body, Device output,
                                   std::size_t header_index) {
    std::size_t index = 0U;
    std::size_t commands = 0U;
    while (index < body.words.size()) {
        require_words(body.words, index, 2U);
        const std::uint8_t command = std_command(body.words[index]);
        std::size_t words = 2U;
        if (command == std_increment) {
            const std::uint8_t subtype = static_cast<std::uint8_t>(
                (body.words[index] >> 20U) & 7U);
            if (subtype >= 4U) {
                words = 4U;
            }
        } else if (command == std_serial_word ||
                   command == std_copy_bytes ||
                   command == std_pointer) {
            words = 4U;
        } else if (command == std_master_test &&
                   is_ar_family(output)) {
            words = 4U;
        }
        if (index + words > body.words.size()) {
            throw TranslationError(
                ErrorCode::invalid_code, header_index,
                "translated conditional body contains a truncated standard multi-line command");
        }
        index += words;
        ++commands;
    }
    return commands;
}

void emit_standard_condition(Cheat& destination, const Cheat& body,
                             const StandardPredicatePlan& predicate,
                             std::uint8_t size,
                             std::uint32_t compare_address, Device output,
                             std::size_t header_index, Report* report) {
    const std::size_t body_commands =
        standard_command_count(body, output, header_index);
    if (body_commands == 0U) {
        append_warning(
            report,
            "An ARMAX condition was removed because its translated body has no runtime effect.");
        return;
    }

    if (predicate.disposition == PredicateDisposition::always_false) {
        append_warning(
            report,
            "An always-false ARMAX condition and its controlled records were removed.");
        return;
    }
    if (predicate.disposition != PredicateDisposition::single) {
        throw TranslationError(ErrorCode::comparison, header_index,
                               "unexpected ARMAX comparison conversion plan");
    }

    const bool cb_layout = uses_cb_condition_layout(output);
    if (size == size_word) {
        throw TranslationError(
            ErrorCode::test_size, header_index,
            "general 32-bit ARMAX comparisons cannot be represented by public D/E codes");
    }
    if (size == size_byte && !cb_layout) {
        throw TranslationError(
            ErrorCode::test_size, header_index,
            "target device does not support 8-bit public D/E comparisons");
    }

    if (body_commands == 1U) {
        const std::uint8_t width = size == size_byte ? 1U : 0U;
        append_pair(destination,
                    make_std(std_test_single) | compare_address,
                    (static_cast<std::uint32_t>(predicate.condition) << 20U) |
                        (static_cast<std::uint32_t>(width) << 16U) |
                        (predicate.value & value_masks[size]));
        append_words(destination, body);
        return;
    }

    if (cb_layout) {
        if (body_commands > 0xFFU) {
            throw TranslationError(
                ErrorCode::count_overflow, header_index,
                "CodeBreaker E has an 8-bit line-count field");
        }
        const std::uint8_t width = size == size_byte ? 1U : 0U;
        append_pair(destination,
                    make_std(std_test_multi) |
                        (static_cast<std::uint32_t>(width) << 24U) |
                        (static_cast<std::uint32_t>(body_commands) << 16U) |
                        (predicate.value & 0xFFFFU),
                    (static_cast<std::uint32_t>(predicate.condition) << 28U) |
                        compare_address);
    } else {
        if (body_commands > 0xFFFU) {
            throw TranslationError(
                ErrorCode::count_overflow, header_index,
                "GameShark/Action Replay E has a 12-bit line-count field");
        }
        append_pair(destination,
                    make_std(std_test_multi) |
                        (static_cast<std::uint32_t>(body_commands) << 16U) |
                        (predicate.value & 0xFFFFU),
                    (static_cast<std::uint32_t>(predicate.condition) << 28U) |
                        compare_address);
    }
    append_words(destination, body);
}

Cheat parse_condition_body(const Cheat& source, std::size_t& index,
                           Device output, std::uint8_t mode,
                           std::size_t header_index, Report* report) {
    if (mode == arm_retry) {
        throw TranslationError(
            ErrorCode::semantic_loss, header_index,
            "ARMAX mode-3 retries the same conditional record until it becomes true");
    }

    Cheat body;
    if (mode == arm_skip_1 || mode == arm_skip_2) {
        const std::size_t record_count =
            mode == arm_skip_1 ? 1U : 2U;
        const std::size_t word_count = record_count * 2U;
        require_words(source.words, index, word_count);
        const std::size_t body_end = index + word_count;
        while (index < body_end) {
            translate_armax_to_standard(body, source, index, output,
                                        report);
            if (index > body_end) {
                throw TranslationError(
                    ErrorCode::invalid_code, header_index,
                    "ARMAX skip count cuts through a multi-record operation");
            }
        }
        return body;
    }

    if (mode != arm_skip_n) {
        throw TranslationError(ErrorCode::unsupported_control,
                               header_index,
                               "unknown ARMAX conditional mode");
    }

    bool found_resume = false;
    while (index < source.words.size()) {
        require_words(source.words, index, 2U);
        if (source.words[index] == arm_special &&
            source.words[index + 1U] == arm_resume) {
            index += 2U;
            found_resume = true;
            break;
        }
        translate_armax_to_standard(body, source, index, output, report);
    }
    if (!found_resume) {
        throw TranslationError(
            ErrorCode::invalid_code, header_index,
            "ARMAX block condition is missing its 00000000 40000000 resume record");
    }
    return body;
}

void emit_word_equal_all_remaining(Cheat& destination, const Cheat& body,
                                   std::uint32_t compare_address,
                                   std::uint32_t compare_value,
                                   Device output, const Cheat& source,
                                   std::size_t index,
                                   std::size_t header_index) {
    if (!uses_cb_condition_layout(output)) {
        throw TranslationError(
            ErrorCode::test_size, header_index,
            "GameShark/Action Replay has no exact equivalent of a 32-bit ARMAX equal gate");
    }
    if (index != source.words.size()) {
        throw TranslationError(
            ErrorCode::semantic_loss, header_index,
            "CodeBreaker type C can control only all remaining records, but this ARMAX 32-bit condition ends before the cheat ends");
    }
    if (body.empty()) {
        return;
    }

    append_pair(destination,
                make_std(std_test_all) | compare_address,
                compare_value);
    append_words(destination, body);
}

void validate_serial_addresses(std::uint32_t start_address,
                               std::uint32_t count,
                               std::int32_t byte_step,
                               std::size_t index) {
    std::int64_t current = start_address;
    for (std::uint32_t item = 0U; item < count; ++item) {
        if (current < 0 || current >
                static_cast<std::int64_t>(address_mask)) {
            throw TranslationError(
                ErrorCode::serial_size, index,
                "ARMAX serial address progression leaves the standard 25-bit address range");
        }
        current += byte_step;
    }
}

void translate_serial(Cheat& destination, const Cheat& source,
                      std::size_t& index, Report* report) {
    require_words(source.words, index, 4U);
    const std::uint32_t descriptor = source.words[index + 1U];
    const std::uint8_t size = arm_size(descriptor);
    if (size > size_word) {
        throw TranslationError(ErrorCode::serial_size, index,
                               "ARMAX serial width 3 is reserved");
    }

    const std::uint32_t start_address = address(descriptor);
    const std::uint32_t start_value = source.words[index + 2U];
    const std::uint32_t packed = source.words[index + 3U];
    const std::int32_t increment =
        static_cast<std::int8_t>(packed >> 24U);
    const std::uint32_t count = ((packed >> 16U) & 0xFFU) + 1U;
    const std::int32_t byte_step =
        static_cast<std::int16_t>(packed & 0xFFFFU);

    if (size == size_word && byte_step >= 0 &&
        (byte_step % 4) == 0) {
        const std::uint32_t step_words =
            static_cast<std::uint32_t>(byte_step / 4);
        append_pair(destination,
                    make_std(std_serial_word) | start_address,
                    (count << 16U) | (step_words & 0xFFFFU));
        append_pair(destination, start_value,
                    static_cast<std::uint32_t>(increment));
    } else {
        validate_serial_addresses(start_address, count, byte_step, index);
        emit_exploded(destination, size, start_address, start_value,
                      count, byte_step, increment);
        append_warning(
            report,
            "An ARMAX byte/halfword or signed-stride serial record expanded into individual standard writes.");
    }
    index += 4U;
}

std::string hook_detail(std::uint32_t config) {
    const std::uint32_t template_selector = (config >> 16U) & 0xFU;
    const std::uint32_t parameter = (config >> 8U) & 0xFFU;
    const std::uint32_t state = config & 0xFU;
    return "ARMAX hook template=" + std::to_string(template_selector) +
           ", parameter=" + std::to_string(parameter) +
           ", state=" + std::to_string(state);
}

} // namespace

void translate_armax_to_standard(Cheat& destination, const Cheat& source,
                                 std::size_t& index, Device output,
                                 Report* report) {
    require_words(source.words, index, 2U);
    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];

    if (first == arm_special) {
        const std::uint8_t selector =
            static_cast<std::uint8_t>(second >> 29U);
        switch (selector) {
            case 0U:
                throw TranslationError(
                    ErrorCode::unsupported_control, index,
                    "ARMAX selector-0 ends the master scan and switches to the normal-code table");
            case 1U:
                index += 2U;
                append_warning(
                    report,
                    "An ARMAX selector-1 reserved/no-op control record was removed.");
                return;
            case 2U:
                throw TranslationError(
                    ErrorCode::unsupported_control, index,
                    "ARMAX resume/conditional-state clear appeared outside a mode-2 block");
            case 3U:
                throw TranslationError(
                    ErrorCode::unsupported_control, index,
                    "ARMAX selector-3 conditional-state control has no standard-device equivalent");
            case 4U:
                translate_serial(destination, source, index, report);
                return;
            default:
                throw TranslationError(
                    ErrorCode::unsupported_control, index,
                    "ARMAX zero-first-word selector 5-7 is unsupported/reserved");
        }
    }

    const std::uint8_t operation = arm_type(first);
    const std::uint8_t subtype = arm_subtype(first);
    const std::uint8_t size = arm_size(first);

    if (operation == arm_write) {
        switch (subtype) {
            case arm_direct: {
                if (size > size_word) {
                    throw TranslationError(
                        ErrorCode::serial_size, index,
                        "ARMAX direct width 3 is reserved");
                }
                std::uint32_t count = 1U;
                std::uint32_t value = second;
                if (size == size_byte) {
                    count = (second >> 8U) + 1U;
                    value = second & 0xFFU;
                } else if (size == size_half) {
                    count = (second >> 16U) + 1U;
                    value = second & 0xFFFFU;
                }
                emit_smash(destination, size, address(first), value,
                           count);
                if (count > 1U) {
                    append_warning(
                        report,
                        "An ARMAX direct fill expanded into standard write/serial records.");
                }
                index += 2U;
                return;
            }

            case arm_pointer: {
                if (size > size_word) {
                    throw TranslationError(
                        ErrorCode::pointer_size, index,
                        "ARMAX pointer width 3 is reserved");
                }
                std::int32_t offset = 0;
                std::uint32_t value = second;
                if (size == size_byte) {
                    offset = sign_extend_24(second >> 8U);
                    value &= 0xFFU;
                } else if (size == size_half) {
                    offset =
                        static_cast<std::int16_t>(second >> 16U) * 2;
                    value &= 0xFFFFU;
                }

                if (is_cb(output) || output == Device::standard) {
                    append_pair(destination,
                                make_std(std_pointer) | address(first),
                                value);
                    append_pair(destination,
                                (static_cast<std::uint32_t>(size) << 16U) |
                                    1U,
                                static_cast<std::uint32_t>(offset));
                } else {
                    if (size == size_byte) {
                        throw TranslationError(
                            ErrorCode::pointer_size, index,
                            "GameShark/Action Replay pointer format has no exact 8-bit mapping");
                    }
                    append_pair(destination,
                                make_std(std_pointer) |
                                    (static_cast<std::uint32_t>(size) <<
                                     24U) |
                                    (address(first) & 0x00FFFFFFU),
                                address(first) & 0x01000000U);
                    append_pair(destination,
                                static_cast<std::uint32_t>(offset), value);
                }
                index += 2U;
                return;
            }

            case arm_increment: {
                if (size == size_special) {
                    throw TranslationError(
                        ErrorCode::value_increment, index,
                        "ARMAX type 86/87 converts the signed integer parameter to float before addition");
                }
                if (size > size_word) {
                    throw TranslationError(ErrorCode::value_increment,
                                           index);
                }

                const std::int32_t delta =
                    static_cast<std::int32_t>(second);
                const bool decrement = delta < 0;
                const std::uint32_t magnitude = decrement
                    ? static_cast<std::uint32_t>(
                          -static_cast<std::int64_t>(delta))
                    : static_cast<std::uint32_t>(delta);

                const std::uint32_t size_bits =
                    static_cast<std::uint32_t>(size) << 1U;
                const std::uint8_t subtype_value = is_ar_family(output)
                    ? static_cast<std::uint8_t>(
                          size_bits + (decrement ? 0U : 1U))
                    : static_cast<std::uint8_t>(
                          size_bits + (decrement ? 1U : 0U));

                if (size == size_word) {
                    append_pair(destination,
                                make_std(std_increment) |
                                    (static_cast<std::uint32_t>(
                                         subtype_value)
                                     << 20U),
                                address(first));
                    append_pair(destination, magnitude, 0U);
                } else {
                    append_pair(destination,
                                make_std(std_increment) |
                                    (static_cast<std::uint32_t>(
                                         subtype_value)
                                     << 20U) |
                                    (magnitude & value_masks[size]),
                                address(first));
                }
                index += 2U;
                return;
            }

            case arm_hook:
                if (size == size_special) {
                    throw TranslationError(
                        ErrorCode::semantic_loss, index,
                        "ARMAX C6/C7 writes through the 12000000 special region, which standard public writes cannot encode exactly");
                }
                if (size == size_word) {
                    const std::uint32_t hook_address = address(first);
                    if (second == arm_enable_std &&
                        hook_address <= address_mask - 3U) {
                        append_pair(destination,
                                    make_std(std_uncond_hook) | hook_address,
                                    hook_address + 3U);
                        index += 2U;
                        return;
                    }
                    throw TranslationError(
                        ErrorCode::unsupported_hook, index,
                        hook_detail(second));
                }
                throw TranslationError(
                    ErrorCode::unsupported_hook, index,
                    "ARMAX C0-C3 hook/setup widths are reserved");
        }
    }

    // Equal-word + same-address C4/C5 using the standard enable template is
    // the exact ARMAX conditional-hook pattern emitted by this translator.
    if (operation == arm_equal && subtype == arm_skip_1 &&
        size == size_word && index + 3U < source.words.size()) {
        const std::uint32_t hook_first = source.words[index + 2U];
        const std::uint32_t hook_config = source.words[index + 3U];
        if (arm_type(hook_first) == arm_write &&
            arm_subtype(hook_first) == arm_hook &&
            arm_size(hook_first) == size_word &&
            address(hook_first) == address(first)) {
            if (hook_config != arm_enable_std) {
                throw TranslationError(
                    ErrorCode::unsupported_hook, index,
                    "conditional ARMAX hook uses a nonstandard template: " +
                        hook_detail(hook_config));
            }
            if (is_ar_family(output)) {
                append_pair(destination,
                            make_std(std_master_test) |
                                (address(first) + 1U),
                            address(first));
                append_pair(destination, second, 0U);
            } else {
                append_pair(destination,
                            make_std(std_cond_hook) | address(first),
                            second);
            }
            index += 4U;
            return;
        }
    }

    if (size == size_special) {
        throw TranslationError(ErrorCode::test_size, index,
                               "ARMAX comparison width 3 is reserved");
    }

    const std::size_t header_index = index;
    index += 2U;
    Cheat body = parse_condition_body(source, index, output, subtype,
                                      header_index, report);
    if (body.empty()) {
        append_warning(
            report,
            "An ARMAX condition was removed because its controlled records have no runtime effect after translation.");
        return;
    }

    const StandardPredicatePlan predicate =
        plan_armax_to_standard_predicate(operation, output, size,
                                         second & value_masks[size],
                                         header_index);

    if (size == size_word) {
        if (predicate.disposition == PredicateDisposition::always_false) {
            append_warning(
                report,
                "An always-false 32-bit ARMAX condition and its body were removed.");
            return;
        }
        if (operation != arm_equal) {
            throw TranslationError(
                ErrorCode::test_size, header_index,
                "only 32-bit ARMAX equality can map to CodeBreaker type C");
        }
        emit_word_equal_all_remaining(
            destination, body, address(first), second, output, source,
            index, header_index);
        return;
    }

    emit_standard_condition(destination, body, predicate, size,
                            address(first), output, header_index, report);
}

} // namespace omni::translate::detail
