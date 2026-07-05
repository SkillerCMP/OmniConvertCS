#include "translate/translation_internal.hpp"

#include "translate/armax_conditions.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace omni::translate::detail {
namespace {

struct ParsedCondition {
    ArmPredicatePlan predicate{};
    std::uint8_t size{};
    std::uint32_t address_value{};
    std::uint32_t value{};
    std::uint32_t command_count{};
};

ParsedCondition parse_single_condition(const Cheat& source, std::size_t index,
                                        Device input) {
    require_words(source.words, index, 2U);
    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];
    const std::uint8_t condition =
        static_cast<std::uint8_t>((second >> 20U) & 7U);

    ParsedCondition parsed;
    parsed.address_value = address(first);
    parsed.value = low_half(second);
    parsed.command_count = 1U;

    if (is_cb(input) || input == Device::standard) {
        const std::uint8_t width =
            static_cast<std::uint8_t>((second >> 16U) & 1U);
        parsed.size = width == 0U ? size_half : size_byte;
    } else {
        parsed.size = size_half;
    }

    parsed.value &= value_masks[parsed.size];
    parsed.predicate = plan_standard_to_armax_predicate(
        condition, input, parsed.size, parsed.value, index);
    return parsed;
}

ParsedCondition parse_multi_condition(const Cheat& source, std::size_t index,
                                       Device input) {
    require_words(source.words, index, 2U);
    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];
    const std::uint8_t condition =
        static_cast<std::uint8_t>((second >> 28U) & 7U);

    ParsedCondition parsed;
    parsed.address_value = address(second);
    parsed.value = low_half(first);

    if (is_cb(input) || input == Device::standard) {
        const std::uint8_t width =
            static_cast<std::uint8_t>((first >> 24U) & 1U);
        parsed.size = width == 0U ? size_half : size_byte;
        parsed.command_count = (first >> 16U) & 0xFFU;
    } else {
        parsed.size = size_half;
        parsed.command_count = (first >> 16U) & 0xFFFU;
    }

    parsed.value &= value_masks[parsed.size];
    parsed.predicate = plan_standard_to_armax_predicate(
        condition, input, parsed.size, parsed.value, index);
    return parsed;
}

void append_words(Cheat& destination, const Cheat& source) {
    for (const std::uint32_t word : source.words) {
        destination.append_word(word);
    }
}

void emit_one_condition(Cheat& destination, std::uint8_t operation,
                        const ParsedCondition& condition, const Cheat& body,
                        std::size_t header_index) {
    const std::size_t body_lines = body.words.size() / 2U;
    if (body_lines == 0U) {
        throw TranslationError(ErrorCode::invalid_code, header_index,
                               "conditional controls zero translated records");
    }

    std::uint8_t mode = arm_skip_n;
    if (body_lines == 1U) {
        mode = arm_skip_1;
    } else if (body_lines == 2U) {
        mode = arm_skip_2;
    }

    append_pair(destination,
                make_arm(operation, mode, condition.size) |
                    condition.address_value,
                condition.value);
    append_words(destination, body);

    if (mode == arm_skip_n) {
        append_pair(destination, arm_special, arm_resume);
    }
}

void emit_condition_plan(Cheat& destination, const ParsedCondition& condition,
                         const Cheat& body, std::size_t header_index,
                         Report* report) {
    switch (condition.predicate.disposition) {
        case PredicateDisposition::single:
            emit_one_condition(destination,
                               condition.predicate.first_operation,
                               condition, body, header_index);
            return;

        case PredicateDisposition::disjunction:
            emit_one_condition(destination,
                               condition.predicate.first_operation,
                               condition, body, header_index);
            emit_one_condition(destination,
                               condition.predicate.second_operation,
                               condition, body, header_index);
            append_warning(
                report,
                "A standard <= or >= comparison expanded into two mutually exclusive ARMAX conditions.");
            return;

        case PredicateDisposition::always_true:
            append_words(destination, body);
            append_warning(
                report,
                "An always-true source comparison was removed while translating to ARMAX.");
            return;

        case PredicateDisposition::always_false:
            append_warning(
                report,
                "An always-false source comparison and its controlled records were removed while translating to ARMAX.");
            return;
    }
}

void translate_controlled_commands(Cheat& destination, const Cheat& source,
                                   std::size_t& index, Device input,
                                   const ParsedCondition& condition,
                                   Report* report) {
    const std::size_t header_index = index;
    index += 2U;

    Cheat body;
    for (std::uint32_t command = 0U;
         command < condition.command_count; ++command) {
        if (index >= source.words.size()) {
            throw TranslationError(
                ErrorCode::invalid_code, header_index,
                "conditional count exceeds the remaining commands");
        }
        translate_standard_to_armax(body, source, index, input, report);
    }

    emit_condition_plan(destination, condition, body, header_index, report);
}

std::uint8_t gs_pointer_size(std::uint32_t first, std::size_t index) {
    const std::uint8_t subtype =
        static_cast<std::uint8_t>((first >> 24U) & 0xFU);
    if (subtype == 0U || subtype == 1U) {
        return size_half;
    }
    if (subtype == 2U) {
        return size_word;
    }
    throw TranslationError(
        ErrorCode::pointer_size, index,
        "GameShark pointer subtypes 63-6F are unsupported; 60/61 are 16-bit aliases and 62 is 32-bit");
}


void append_armax_serial(Cheat& destination, std::uint8_t size,
                         std::uint32_t start_address,
                         std::uint32_t start_value,
                         std::uint32_t count,
                         std::int32_t byte_step,
                         std::int32_t increment,
                         std::size_t index) {
    if (size > size_word || count == 0U || count > 256U) {
        throw TranslationError(ErrorCode::serial_size, index,
                               "invalid ARMAX serial descriptor");
    }
    if (start_address > address_mask) {
        throw TranslationError(
            ErrorCode::serial_size, index,
            "ARMAX serial chunk start exceeds the 25-bit address field");
    }
    if (byte_step < std::numeric_limits<std::int16_t>::min() ||
        byte_step > std::numeric_limits<std::int16_t>::max()) {
        throw TranslationError(ErrorCode::serial_size, index,
                               "ARMAX serial byte stride must fit signed 16-bit");
    }
    if (increment < std::numeric_limits<std::int8_t>::min() ||
        increment > std::numeric_limits<std::int8_t>::max()) {
        throw TranslationError(ErrorCode::value_increment, index,
                               "ARMAX serial increment must fit signed 8-bit");
    }

    const std::uint32_t packed =
        (static_cast<std::uint32_t>(
             static_cast<std::uint8_t>(increment))
         << 24U) |
        ((count - 1U) << 16U) |
        static_cast<std::uint16_t>(byte_step);

    append_pair(destination, arm_special,
                make_arm(arm_write, arm_increment, size) |
                    start_address);
    append_pair(destination, start_value, packed);
}

std::uint32_t advanced_value(std::uint32_t start_value,
                             std::int32_t increment,
                             std::uint32_t item_count) noexcept {
    return start_value +
           static_cast<std::uint32_t>(increment) * item_count;
}

void emit_armax_direct_write(Cheat& destination, std::uint8_t size,
                             std::uint32_t address_value,
                             std::uint32_t value,
                             std::size_t index,
                             Report* report) {
    // 00000000 is the ARMAX special/control prefix. A normal byte write to
    // address zero would collide with that prefix, so use a one-item serial
    // descriptor to preserve the write exactly.
    if (size == size_byte && address_value == 0U) {
        append_armax_serial(destination, size_byte, 0U, value & 0xFFU,
                            1U, 1, 0, index);
        append_warning(
            report,
            "A byte write to address 00000000 was encoded as a one-item ARMAX serial record to avoid the zero-word control prefix.");
        return;
    }

    append_pair(destination,
                make_arm(arm_write, arm_direct, size) | address_value,
                value & value_masks[size]);
}

void translate_standard_serial(Cheat& destination, const Cheat& source,
                               std::size_t& index, Report* report) {
    require_words(source.words, index, 4U);
    const std::uint32_t first = source.words[index];
    const std::uint32_t descriptor = source.words[index + 1U];
    const std::uint32_t start_value = source.words[index + 2U];
    const std::int32_t increment =
        static_cast<std::int32_t>(source.words[index + 3U]);
    const std::uint32_t count = high_half(descriptor);
    const std::uint32_t step_words = low_half(descriptor);

    if (count == 0U) {
        throw TranslationError(
            ErrorCode::count_overflow, index,
            "standard type-4 serial count must be at least one");
    }

    const std::uint64_t byte_step64 =
        static_cast<std::uint64_t>(step_words) * 4U;
    const bool serial_fields_fit =
        byte_step64 <=
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int16_t>::max()) &&
        increment >= std::numeric_limits<std::int8_t>::min() &&
        increment <= std::numeric_limits<std::int8_t>::max();

    if (serial_fields_fit) {
        std::uint32_t remaining = count;
        std::uint32_t processed = 0U;
        std::uint32_t chunks = 0U;

        // Put the short remainder chunk first. This keeps every later chunk
        // start as low as possible and therefore preserves more valid ranges
        // near the top of ARMAX's 25-bit address field.
        std::uint32_t next_chunk = count % 256U;
        if (next_chunk == 0U) next_chunk = 256U;

        while (remaining != 0U) {
            const std::uint32_t chunk_count = next_chunk;
            const std::uint64_t chunk_address64 =
                static_cast<std::uint64_t>(address(first)) +
                static_cast<std::uint64_t>(processed) * byte_step64;
            if (chunk_address64 > address_mask) {
                throw TranslationError(
                    ErrorCode::serial_size, index,
                    "splitting the standard serial requires an ARMAX chunk start outside the 25-bit address field");
            }

            append_armax_serial(
                destination, size_word,
                static_cast<std::uint32_t>(chunk_address64),
                advanced_value(start_value, increment, processed),
                chunk_count, static_cast<std::int32_t>(byte_step64),
                increment, index);

            processed += chunk_count;
            remaining -= chunk_count;
            next_chunk = remaining > 256U ? 256U : remaining;
            ++chunks;
        }

        if (chunks > 1U) {
            append_warning(
                report,
                "A standard serial record was split into multiple ARMAX serial records because ARMAX stores an 8-bit count-minus-one field.");
        }
        index += 4U;
        return;
    }

    for (std::uint32_t item = 0U; item < count; ++item) {
        const std::uint64_t item_address64 =
            static_cast<std::uint64_t>(address(first)) +
            static_cast<std::uint64_t>(item) * byte_step64;
        if (item_address64 > address_mask) {
            throw TranslationError(
                ErrorCode::serial_size, index,
                "expanding the standard serial requires an ARMAX write address outside the 25-bit field");
        }
        emit_armax_direct_write(
            destination, size_word,
            static_cast<std::uint32_t>(item_address64),
            advanced_value(start_value, increment, item), index, report);
    }

    append_warning(
        report,
        "A standard serial record expanded into individual ARMAX word writes because its stride or value increment does not fit the ARMAX serial descriptor.");
    index += 4U;
}

} // namespace

void translate_standard_to_armax(Cheat& destination, const Cheat& source,
                                 std::size_t& index, Device input,
                                 Report* report) {
    require_words(source.words, index, 2U);
    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];
    const std::uint8_t command = std_command(first);

    switch (command) {
        case std_write_byte:
        case std_write_half:
        case std_write_word:
            emit_armax_direct_write(destination, command, address(first),
                                    second, index, report);
            index += 2U;
            return;

        case std_increment: {
            const std::uint8_t subtype =
                static_cast<std::uint8_t>((first >> 20U) & 7U);
            if (subtype >= 7U) {
                throw TranslationError(ErrorCode::invalid_code, index,
                                       "invalid increment subtype");
            }

            std::uint8_t size = 0U;
            bool decrement = false;
            if (is_ar_family(input)) {
                size = ar_increment_sizes[subtype];
                decrement = (subtype & 1U) == 0U;
            } else {
                size = standard_increment_sizes[subtype];
                decrement = (subtype & 1U) != 0U;
            }

            std::uint32_t amount = 0U;
            if (size == size_word) {
                require_words(source.words, index, 4U);
                amount = source.words[index + 2U];
                index += 4U;
            } else {
                amount = first & value_masks[size];
                index += 2U;
            }

            const std::uint32_t signed_amount =
                decrement ? (~amount + 1U) : amount;
            append_pair(destination,
                        make_arm(arm_write, arm_increment, size) |
                            address(second),
                        signed_amount);
            return;
        }

        case std_serial_word:
            translate_standard_serial(destination, source, index, report);
            return;

        case std_copy_bytes:
            require_words(source.words, index, 4U);
            throw TranslationError(ErrorCode::copy_bytes, index);

        case std_pointer: {
            require_words(source.words, index, 4U);
            std::uint8_t size = 0U;
            std::uint32_t pointer_address = 0U;
            std::uint32_t value = 0U;
            std::int64_t offset = 0;

            if (is_cb(input) || input == Device::standard) {
                const std::uint32_t descriptor = source.words[index + 2U];
                size = static_cast<std::uint8_t>(high_half(descriptor) & 3U);
                const std::uint16_t count = low_half(descriptor);
                if (count != 1U) {
                    throw TranslationError(
                        ErrorCode::excess_offsets, index,
                        "ARMAX supports exactly one dereference and one final offset");
                }
                if (size > size_word) {
                    throw TranslationError(ErrorCode::pointer_size, index);
                }
                pointer_address = address(first);
                value = second & value_masks[size];
                offset =
                    static_cast<std::int32_t>(source.words[index + 3U]);
            } else {
                size = gs_pointer_size(first, index);
                const std::uint64_t full_address =
                    static_cast<std::uint64_t>(first & 0x00FFFFFFU) +
                    static_cast<std::uint64_t>(second);
                if (full_address > address_mask) {
                    throw TranslationError(
                        ErrorCode::offset_value_32, index,
                        "pointer load offset produces an address outside ARMAX's 25-bit field");
                }
                pointer_address = static_cast<std::uint32_t>(full_address);
                value = source.words[index + 3U] & value_masks[size];
                offset =
                    static_cast<std::int32_t>(source.words[index + 2U]);
            }

            std::uint32_t packed = value;
            if (size == size_byte) {
                if (offset < -0x800000LL || offset > 0x7FFFFFLL) {
                    throw TranslationError(ErrorCode::offset_value_8, index);
                }
                packed =
                    (static_cast<std::uint32_t>(offset) & 0x00FFFFFFU)
                    << 8U;
                packed |= value & 0xFFU;
            } else if (size == size_half) {
                if ((offset & 1LL) != 0LL) {
                    throw TranslationError(
                        ErrorCode::offset_value_16, index,
                        "ARMAX 16-bit pointer offsets are stored in two-byte units");
                }
                const std::int64_t units = offset / 2LL;
                if (units < -0x8000LL || units > 0x7FFFLL) {
                    throw TranslationError(ErrorCode::offset_value_16,
                                           index);
                }
                packed =
                    (static_cast<std::uint32_t>(units) & 0xFFFFU) << 16U;
                packed |= value & 0xFFFFU;
            } else if (offset != 0) {
                throw TranslationError(ErrorCode::offset_value_32, index);
            }

            append_pair(destination,
                        make_arm(arm_write, arm_pointer, size) |
                            pointer_address,
                        packed);
            index += 4U;
            return;
        }

        case std_bitwise:
            throw TranslationError(ErrorCode::bitwise, index);

        case std_master_test:
            if (!is_ar_family(input)) {
                throw TranslationError(
                    ErrorCode::semantic_loss, index,
                    "CB/GS type-8 setup gates are not runtime ARMAX conditionals");
            }
            require_words(source.words, index, 4U);
            append_pair(destination,
                        make_arm(arm_equal, arm_skip_1, size_word) |
                            address(source.words[index + 1U]),
                        source.words[index + 2U]);
            append_pair(destination,
                        make_arm(arm_write, arm_hook, size_word) |
                            address(source.words[index + 1U]),
                        arm_enable_std);
            index += 4U;
            return;

        case std_cond_hook:
            append_pair(destination,
                        make_arm(arm_equal, arm_skip_1, size_word) |
                            address(first),
                        second);
            append_pair(destination,
                        make_arm(arm_write, arm_hook, size_word) |
                            address(first),
                        arm_enable_std);
            index += 2U;
            return;

        case std_once_write:
            throw TranslationError(
                ErrorCode::semantic_loss, index,
                "type A is a one-time setup patch; an ARMAX direct write is persistent");

        case std_timer:
            throw TranslationError(
                ErrorCode::timer, index,
                "CodeBreaker B controls and GameShark B delays have no exact ARMAX control mapping");

        case std_test_all: {
            if (is_gs(input)) {
                throw TranslationError(
                    ErrorCode::semantic_loss, index,
                    "GameShark C is a wait/control gate, not a persistent ARMAX block condition");
            }

            const std::size_t header_index = index;
            index += 2U;
            Cheat body;
            while (index < source.words.size()) {
                translate_standard_to_armax(body, source, index, input,
                                            report);
            }
            if (body.empty()) {
                throw TranslationError(
                    ErrorCode::invalid_code, header_index,
                    "type C has no remaining records to control");
            }

            append_pair(destination,
                        make_arm(arm_equal, arm_skip_n, size_word) |
                            address(first),
                        second);
            append_words(destination, body);
            append_pair(destination, arm_special, arm_resume);
            return;
        }

        case std_test_single: {
            const ParsedCondition condition =
                parse_single_condition(source, index, input);
            translate_controlled_commands(destination, source, index, input,
                                          condition, report);
            return;
        }

        case std_test_multi: {
            const ParsedCondition condition =
                parse_multi_condition(source, index, input);
            if (condition.command_count == 0U) {
                throw TranslationError(ErrorCode::invalid_code, index,
                                       "conditional count is zero");
            }
            translate_controlled_commands(destination, source, index, input,
                                          condition, report);
            return;
        }

        case std_uncond_hook: {
            // The original OmniConvert mapping recognizes the canonical
            // CodeJunkies/standard unconditional-hook form as:
            //
            //   F0aaaaaa 00aaaaa3
            //
            // where the second word is the 25-bit hook address plus three.
            // ARMAX represents that same public hook form with the standard
            // C4 template. Do not generalize this to arbitrary type-F mode or
            // parameter values because those fields cannot be reconstructed.
            const std::uint32_t hook_address = address(first);
            if (hook_address <= address_mask - 3U &&
                second == hook_address + 3U) {
                append_pair(destination,
                            make_arm(arm_write, arm_hook, size_word) |
                                hook_address,
                            arm_enable_std);
                index += 2U;
                return;
            }

            throw TranslationError(
                ErrorCode::unsupported_hook, index,
                "standard type-F hook is not the canonical address-plus-three form required for ARMAX C4 0003FF00");
        }
    }

    throw TranslationError(ErrorCode::invalid_code, index,
                           "unknown standard command type");
}

} // namespace omni::translate::detail
