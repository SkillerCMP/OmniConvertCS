#include "translate/armax_conditions.hpp"

#include "translate/translation_internal.hpp"

namespace omni::translate::detail {
namespace {

[[nodiscard]] std::uint32_t width_mask(std::uint8_t size,
                                       std::size_t index) {
    if (size > size_word) {
        throw TranslationError(ErrorCode::test_size, index,
                               "ARMAX comparison width 3 is reserved");
    }
    return value_masks[size];
}

[[nodiscard]] bool uses_cb_condition_table(Device device) noexcept {
    return is_cb(device) || device == Device::standard;
}

[[nodiscard]] bool uses_legacy_ar_condition_table(Device device) noexcept {
    return is_ar_family(device);
}

} // namespace

ArmPredicatePlan plan_standard_to_armax_predicate(
    std::uint8_t condition, Device input, std::uint8_t size,
    std::uint32_t value, std::size_t index) {
    const std::uint32_t mask = width_mask(size, index);
    value &= mask;

    if (condition == 0U) {
        return {PredicateDisposition::single, arm_equal, 0U};
    }
    if (condition == 1U) {
        return {PredicateDisposition::single, arm_not_equal, 0U};
    }

    if (uses_cb_condition_table(input)) {
        switch (condition) {
            case 2U: // unsigned memory <= value
                if (value == mask) {
                    return {PredicateDisposition::always_true, 0U, 0U};
                }
                return {PredicateDisposition::disjunction,
                        arm_less_unsigned, arm_equal};
            case 3U: // unsigned memory < value
                if (value == 0U) {
                    return {PredicateDisposition::always_false, 0U, 0U};
                }
                return {PredicateDisposition::single,
                        arm_less_unsigned, 0U};
            case 5U: // (memory & value) != 0
                if (value == 0U) {
                    return {PredicateDisposition::always_false, 0U, 0U};
                }
                return {PredicateDisposition::single, arm_and, 0U};
            case 4U:
            case 6U:
            case 7U:
                throw TranslationError(
                    ErrorCode::comparison, index,
                    "CodeBreaker NAND/NOR/OR tests have no exact ARMAX predicate");
            default:
                throw TranslationError(ErrorCode::comparison, index,
                                       "invalid CodeBreaker comparison selector");
        }
    }

    if (is_gs(input)) {
        switch (condition) {
            case 2U: // unsigned memory <= value
                if (value == mask) {
                    return {PredicateDisposition::always_true, 0U, 0U};
                }
                return {PredicateDisposition::disjunction,
                        arm_less_unsigned, arm_equal};
            case 3U: // unsigned memory >= value
                if (value == 0U) {
                    return {PredicateDisposition::always_true, 0U, 0U};
                }
                return {PredicateDisposition::disjunction,
                        arm_greater_unsigned, arm_equal};
            default:
                throw TranslationError(
                    ErrorCode::comparison, index,
                    "GameShark V3/V4 supports only comparison selectors 0-3");
        }
    }

    if (uses_legacy_ar_condition_table(input)) {
        switch (condition) {
            case 2U:
                if (value == 0U) {
                    return {PredicateDisposition::always_false, 0U, 0U};
                }
                return {PredicateDisposition::single,
                        arm_less_unsigned, 0U};
            case 3U:
                if (value == mask) {
                    return {PredicateDisposition::always_false, 0U, 0U};
                }
                return {PredicateDisposition::single,
                        arm_greater_unsigned, 0U};
            case 5U:
                if (value == 0U) {
                    return {PredicateDisposition::always_false, 0U, 0U};
                }
                return {PredicateDisposition::single, arm_and, 0U};
            default:
                throw TranslationError(
                    ErrorCode::comparison, index,
                    "Action Replay comparison selector has no confirmed ARMAX mapping");
        }
    }

    throw TranslationError(ErrorCode::comparison, index,
                           "unknown source-device comparison table");
}

StandardPredicatePlan plan_armax_to_standard_predicate(
    std::uint8_t operation, Device output, std::uint8_t size,
    std::uint32_t value, std::size_t index) {
    const std::uint32_t mask = width_mask(size, index);
    value &= mask;

    if (operation == arm_equal) {
        return {PredicateDisposition::single, 0U, value};
    }
    if (operation == arm_not_equal) {
        return {PredicateDisposition::single, 1U, value};
    }
    if (operation == arm_less_signed || operation == arm_greater_signed) {
        throw TranslationError(
            ErrorCode::comparison, index,
            "signed ARMAX comparisons cannot be changed to unsigned comparisons");
    }

    // These predicates are mathematically false for every value of the
    // selected width. Simplify them before consulting the target device's
    // comparison table so that an unavailable predicate does not block an
    // exact no-op conversion.
    if ((operation == arm_less_unsigned && value == 0U) ||
        (operation == arm_greater_unsigned && value == mask) ||
        (operation == arm_and && value == 0U)) {
        return {PredicateDisposition::always_false, 0U, 0U};
    }

    if (uses_cb_condition_table(output)) {
        if (operation == arm_less_unsigned) {
            return {PredicateDisposition::single, 3U, value};
        }
        if (operation == arm_and) {
            return {PredicateDisposition::single, 5U, value};
        }
        if (operation == arm_greater_unsigned) {
            throw TranslationError(
                ErrorCode::comparison, index,
                "CodeBreaker has no single public D/E predicate for unsigned greater-than");
        }
    } else if (is_gs(output)) {
        if (operation == arm_less_unsigned) {
            return {PredicateDisposition::single, 2U, value - 1U};
        }
        if (operation == arm_greater_unsigned) {
            return {PredicateDisposition::single, 3U, value + 1U};
        }
        if (operation == arm_and) {
            throw TranslationError(
                ErrorCode::comparison, index,
                "GameShark V3/V4 has no public bit-mask comparison equivalent to ARMAX AND");
        }
    } else if (uses_legacy_ar_condition_table(output)) {
        if (operation == arm_less_unsigned) {
            return {PredicateDisposition::single, 2U, value};
        }
        if (operation == arm_greater_unsigned) {
            return {PredicateDisposition::single, 3U, value};
        }
        if (operation == arm_and) {
            return {PredicateDisposition::single, 5U, value};
        }
    }

    throw TranslationError(
        ErrorCode::comparison, index,
        "target device has no exact public comparison for this ARMAX operation");
}

} // namespace omni::translate::detail
