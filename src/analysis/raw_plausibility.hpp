#pragma once

#include "core/cheat.hpp"
#include "core/crypt.hpp"

#include <cstdint>
#include <string>

namespace omni::analysis {

enum class AnalyzeMode { none, analyze, analyze_fix };

struct PlausibilityResult {
    bool valid{};
    std::uint32_t checked_word{};
    std::uint32_t checked_addr28{};
    std::string rule;
    std::string reason;
    std::string note;
};

[[nodiscard]] PlausibilityResult validate_raw_plausibility(
    const Cheat& cheat, Crypt output_crypt = Crypt::raw,
    AnalyzeMode mode = AnalyzeMode::analyze);

[[nodiscard]] bool is_raw_family(Crypt crypt) noexcept;

} // namespace omni::analysis
