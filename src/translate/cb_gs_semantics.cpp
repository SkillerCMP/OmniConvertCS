#include "translate/cb_gs_semantics.hpp"

#include "translate/translation_internal.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace omni::translate::detail {
namespace {

constexpr std::uint32_t public_field_mask = 0x0FFFFFFFU;
constexpr std::uint32_t public_aligned_mask = 0x0FFFFFFCU;
constexpr std::uint32_t gs_pointer_address_mask = 0x00FFFFFFU;

void copy_pairs(Cheat& destination, const Cheat& source, std::size_t index,
                std::size_t pair_count) {
    const std::size_t word_count = pair_count * 2U;
    require_words(source.words, index, word_count);
    for (std::size_t offset = 0U; offset < word_count; ++offset) {
        destination.append_word(source.words[index + offset]);
    }
}

[[nodiscard]] std::uint8_t cb_condition(std::uint32_t second) noexcept {
    return static_cast<std::uint8_t>((second >> 20U) & 7U);
}

[[nodiscard]] std::uint8_t gs_condition(std::uint32_t second) noexcept {
    return static_cast<std::uint8_t>((second >> 20U) & 0xFU);
}

[[nodiscard]] std::uint8_t cb_multi_condition(std::uint32_t second) noexcept {
    return static_cast<std::uint8_t>((second >> 28U) & 7U);
}

[[nodiscard]] std::uint8_t gs_multi_condition(std::uint32_t second) noexcept {
    return static_cast<std::uint8_t>((second >> 28U) & 0xFU);
}

struct ConvertedComparison {
    std::uint8_t condition{};
    std::uint16_t value{};
    bool remove_condition{};
};

ConvertedComparison convert_cb_to_gs_comparison(std::uint8_t condition,
                                                 std::uint16_t value,
                                                 std::size_t index,
                                                 Report* report) {
    switch (condition) {
        case 0U: // equal
        case 1U: // not equal
        case 2U: // unsigned memory <= value
            return {condition, value, false};
        case 3U: // unsigned memory < value
            if (value == 0U) {
                throw TranslationError(
                    ErrorCode::comparison, index,
                    "CodeBreaker unsigned < 0000 is always false and has no single GameShark condition encoding");
            }
            append_warning(
                report,
                "CodeBreaker unsigned less-than was represented exactly as GameShark unsigned less-than-or-equal with value minus one.");
            return {2U, static_cast<std::uint16_t>(value - 1U), false};
        case 4U:
        case 5U:
        case 6U:
        case 7U:
            throw TranslationError(
                ErrorCode::comparison, index,
                "GameShark V3/V4 supports only equal, not-equal, unsigned <=, and unsigned >= tests");
        default:
            throw TranslationError(ErrorCode::comparison, index,
                                   "invalid CodeBreaker comparison selector");
    }
}

ConvertedComparison convert_gs_to_cb_comparison(std::uint8_t condition,
                                                 std::uint16_t value,
                                                 std::size_t index,
                                                 Report* report) {
    switch (condition) {
        case 0U: // equal
        case 1U: // not equal
        case 2U: // unsigned memory <= value
            return {condition, value, false};
        case 3U: // unsigned memory >= value
            if (value == 0U) {
                append_warning(
                    report,
                    "GameShark unsigned >= 0000 condition was removed because it is always true for a 16-bit value.");
                return {0U, 0U, true};
            }
            throw TranslationError(
                ErrorCode::comparison, index,
                "GameShark unsigned >= cannot be represented by one CodeBreaker public D/E condition");
        default:
            throw TranslationError(ErrorCode::comparison, index,
                                   "GameShark comparison selector is outside the supported 0-3 range");
    }
}

ConvertedComparison convert_comparison(Device input, std::uint8_t condition,
                                       std::uint16_t value, std::size_t index,
                                       Report* report) {
    if (is_cb(input)) {
        return convert_cb_to_gs_comparison(condition, value, index, report);
    }
    return convert_gs_to_cb_comparison(condition, value, index, report);
}

void translate_increment(Cheat& destination, const Cheat& source,
                         std::size_t& index) {
    require_words(source.words, index, 2U);
    const std::uint8_t subtype =
        static_cast<std::uint8_t>((source.words[index] >> 20U) & 7U);
    if (subtype > 5U) {
        throw TranslationError(ErrorCode::invalid_code, index,
                               "CodeBreaker/GameShark increment subtype must be 0-5");
    }

    const std::size_t pairs = subtype >= 4U ? 2U : 1U;
    copy_pairs(destination, source, index, pairs);
    index += pairs * 2U;
}

void translate_pointer(Cheat& destination, const Cheat& source,
                       std::size_t& index, Device input) {
    require_words(source.words, index, 4U);

    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];
    const std::uint32_t third = source.words[index + 2U];
    const std::uint32_t fourth = source.words[index + 3U];

    if (is_cb(input)) {
        const std::uint16_t count = low_half(third);
        const std::uint8_t size =
            static_cast<std::uint8_t>((third >> 16U) & 3U);

        if (count != 1U) {
            throw TranslationError(
                ErrorCode::excess_offsets, index,
                "GameShark 60/61/62 has exactly one dereference and one final offset");
        }
        if (size != size_half && size != size_word) {
            throw TranslationError(
                ErrorCode::pointer_size, index,
                "GameShark 60/61/62 has no exact CodeBreaker 8-bit or special-width pointer mapping");
        }

        const std::uint32_t base = address(first);
        const std::uint32_t gs_subtype = size == size_word ? 2U : 1U;
        append_pair(destination,
                    make_std(std_pointer) | (gs_subtype << 24U) |
                        (base & gs_pointer_address_mask),
                    base & 0x01000000U);
        append_pair(destination, fourth & public_field_mask,
                    second & value_masks[size]);
    } else {
        const std::uint8_t subtype =
            static_cast<std::uint8_t>((first >> 24U) & 0xFU);
        std::uint8_t size = 0U;
        if (subtype == 0U || subtype == 1U) {
            size = size_half;
        } else if (subtype == 2U) {
            size = size_word;
        } else {
            throw TranslationError(
                ErrorCode::pointer_size, index,
                "GameShark pointer subtypes 63-6F are unsupported; 60/61 are 16-bit aliases and 62 is 32-bit");
        }

        const std::uint64_t base =
            static_cast<std::uint64_t>(first & gs_pointer_address_mask) +
            static_cast<std::uint64_t>(second);
        if (base > static_cast<std::uint64_t>(address_mask)) {
            throw TranslationError(
                ErrorCode::offset_value_32, index,
                "GameShark load offset produces a base address outside CodeBreaker's 25-bit address field");
        }

        append_pair(destination,
                    make_std(std_pointer) | static_cast<std::uint32_t>(base),
                    fourth & value_masks[size]);
        append_pair(destination,
                    1U | (static_cast<std::uint32_t>(size) << 16U),
                    third & public_field_mask);
    }

    index += 4U;
}

void translate_type8(Cheat& destination, std::uint32_t first,
                     std::uint32_t second, std::size_t& index) {
    append_pair(destination, make_std(std_master_test) |
                                 ((first & public_field_mask) & ~1U),
                second);
    index += 2U;
}

void translate_type9(Cheat& destination, std::uint32_t first,
                     std::uint32_t second, std::size_t& index) {
    append_pair(destination,
                make_std(std_cond_hook) | (first & public_aligned_mask),
                second);
    index += 2U;
}

void translate_type_a(Cheat& destination, std::uint32_t first,
                      std::uint32_t second, std::size_t& index,
                      Device input) {
    if (is_gs(input) && (first & 1U) != 0U) {
        throw TranslationError(
            ErrorCode::semantic_loss, index,
            "GameShark type A low bit 1 retains the setup record, while CodeBreaker type A is always consumed after setup");
    }

    append_pair(destination,
                make_std(std_once_write) |
                    ((first & public_field_mask) & ~1U),
                second);
    index += 2U;
}

void translate_type_b(Cheat& destination, std::uint32_t first,
                      std::uint32_t second, std::size_t& index,
                      Device input) {
    if (is_cb(input)) {
        const std::uint8_t subtype = static_cast<std::uint8_t>(first & 0xFU);
        if (subtype != 0U) {
            std::string detail;
            switch (subtype) {
                case 1U:
                    detail = "CodeBreaker B1 clears the engine-installed flag";
                    break;
                case 2U:
                    detail = "CodeBreaker B2 is an unsafe/unresolved internal control";
                    break;
                case 3U:
                    detail = "CodeBreaker B3 replaces the stored hook address";
                    break;
                default:
                    detail = "CodeBreaker B subtype is not the countdown/delay subtype";
                    break;
            }
            throw TranslationError(ErrorCode::unsupported_control, index,
                                   std::move(detail));
        }
    }

    // GS ignores every field in its first public word and always emits B0.
    append_pair(destination, make_std(std_timer), second);
    index += 2U;
}

void translate_type_c(std::size_t index) {
    throw TranslationError(
        ErrorCode::semantic_loss, index,
        "CodeBreaker C is a persistent all-remaining-lines gate; GameShark C waits for equality, clears itself, then continues");
}

void translate_type_d(Cheat& destination, std::uint32_t first,
                      std::uint32_t second, std::size_t& index,
                      Device input, Report* report) {
    std::uint8_t condition = 0U;
    if (is_cb(input)) {
        const std::uint8_t width =
            static_cast<std::uint8_t>((second >> 16U) & 1U);
        if (width != 0U) {
            throw TranslationError(
                ErrorCode::test_size, index,
                "GameShark D conditions are fixed 16-bit and cannot preserve a CodeBreaker 8-bit condition");
        }
        condition = cb_condition(second);
    } else {
        condition = gs_condition(second);
    }

    const ConvertedComparison converted = convert_comparison(
        input, condition, low_half(second), index, report);
    index += 2U;
    if (converted.remove_condition) return;

    append_pair(destination,
                make_std(std_test_single) | address(first),
                (static_cast<std::uint32_t>(converted.condition) << 20U) |
                    converted.value);
}

void translate_type_e(Cheat& destination, std::uint32_t first,
                      std::uint32_t second, std::size_t& index,
                      Device input, Report* report) {
    std::uint32_t count = 0U;
    std::uint8_t condition = 0U;

    if (is_cb(input)) {
        const std::uint8_t width =
            static_cast<std::uint8_t>((first >> 24U) & 1U);
        if (width != 0U) {
            throw TranslationError(
                ErrorCode::test_size, index,
                "GameShark E conditions are fixed 16-bit and cannot preserve a CodeBreaker 8-bit condition");
        }
        count = (first >> 16U) & 0xFFU;
        condition = cb_multi_condition(second);
    } else {
        count = (first >> 16U) & 0xFFFU;
        condition = gs_multi_condition(second);
        if (count > 0xFFU) {
            throw TranslationError(
                ErrorCode::count_overflow, index,
                "CodeBreaker E has an 8-bit controlled-record count");
        }
    }

    const ConvertedComparison converted = convert_comparison(
        input, condition, low_half(first), index, report);
    index += 2U;
    if (converted.remove_condition) return;

    append_pair(destination,
                make_std(std_test_multi) | (count << 16U) |
                    converted.value,
                (static_cast<std::uint32_t>(converted.condition) << 28U) |
                    address(second));
}

void translate_type_f(Cheat& destination, std::uint32_t first,
                      std::uint32_t second, std::size_t& index,
                      Device input, Report* report) {
    if (first == std::numeric_limits<std::uint32_t>::max()) {
        index += 2U;
        if (is_cb(input)) {
            append_warning(
                report,
                "CodeBreaker FFFFFFFF setup record was removed because the CodeBreaker parser consumes it as a no-op.");
            return;
        }

        throw TranslationError(
            ErrorCode::unsupported_hook, index - 2U,
            "GameShark treats FFFFFFFF as a real type-F setup record, while CodeBreaker consumes that form as a no-op");
    }

    if (is_gs(input) && (first & 3U) != 0U) {
        throw TranslationError(
            ErrorCode::unsupported_hook, index,
            "GameShark preserves the low two primary-hook address bits, but CodeBreaker aligns the field to four bytes");
    }

    // Both devices use the same low-two-bit mode and parameter transform:
    // modes 0-2 store parameter>>2, mode 3 stores the aligned secondary hook.
    append_pair(destination,
                make_std(std_uncond_hook) | (first & public_aligned_mask),
                second);
    index += 2U;
}

} // namespace

bool is_codebreaker_gameshark_pair(Device input, Device output) noexcept {
    return (is_cb(input) && is_gs(output)) ||
           (is_gs(input) && is_cb(output));
}

void translate_codebreaker_gameshark(Cheat& destination,
                                     const Cheat& source,
                                     std::size_t& index,
                                     Device input,
                                     Device output,
                                     Report* report) {
    (void)output;
    require_words(source.words, index, 2U);
    const std::uint32_t first = source.words[index];
    const std::uint32_t second = source.words[index + 1U];

    switch (std_command(first)) {
        case std_write_byte:
        case std_write_half:
        case std_write_word:
            append_pair(destination, first, second);
            index += 2U;
            return;

        case std_increment:
            translate_increment(destination, source, index);
            return;

        case std_serial_word:
        case std_copy_bytes:
            copy_pairs(destination, source, index, 2U);
            index += 4U;
            return;

        case std_pointer:
            translate_pointer(destination, source, index, input);
            return;

        case std_bitwise:
            throw TranslationError(
                ErrorCode::bitwise, index,
                "CodeBreaker type 7 is a bitwise operation, while GameShark type 7 is verifier/seed metadata");

        case std_master_test:
            translate_type8(destination, first, second, index);
            return;

        case std_cond_hook:
            translate_type9(destination, first, second, index);
            return;

        case std_once_write:
            translate_type_a(destination, first, second, index, input);
            return;

        case std_timer:
            translate_type_b(destination, first, second, index, input);
            return;

        case std_test_all:
            translate_type_c(index);
            return;

        case std_test_single:
            translate_type_d(destination, first, second, index, input, report);
            return;

        case std_test_multi:
            translate_type_e(destination, first, second, index, input, report);
            return;

        case std_uncond_hook:
            translate_type_f(destination, first, second, index, input, report);
            return;
    }

    throw TranslationError(ErrorCode::invalid_code, index,
                           "unknown CodeBreaker/GameShark public command type");
}

} // namespace omni::translate::detail
