#pragma once

#include "core/crypt.hpp"

#include <cstddef>
#include <cstdint>

namespace omni::translate::detail {

enum class PredicateDisposition {
    single,
    disjunction,
    always_true,
    always_false
};

struct ArmPredicatePlan {
    PredicateDisposition disposition{PredicateDisposition::single};
    std::uint8_t first_operation{};
    std::uint8_t second_operation{};
};

struct StandardPredicatePlan {
    PredicateDisposition disposition{PredicateDisposition::single};
    std::uint8_t condition{};
    std::uint32_t value{};
};

[[nodiscard]] ArmPredicatePlan plan_standard_to_armax_predicate(
    std::uint8_t condition, Device input, std::uint8_t size,
    std::uint32_t value, std::size_t index);

[[nodiscard]] StandardPredicatePlan plan_armax_to_standard_predicate(
    std::uint8_t operation, Device output, std::uint8_t size,
    std::uint32_t value, std::size_t index);

} // namespace omni::translate::detail
