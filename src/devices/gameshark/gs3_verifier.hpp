#pragma once

#include "core/cheat.hpp"

#include <cstdint>
#include <vector>

namespace omni::devices::gameshark {

[[nodiscard]] std::uint16_t ccitt_crc_words(
    const std::vector<std::uint32_t>& words);
[[nodiscard]] CodePair create_verifier(
    const std::vector<std::uint32_t>& words);
[[nodiscard]] bool looks_like_verifier(CodePair code) noexcept;
[[nodiscard]] bool has_valid_verifier(
    const std::vector<std::uint32_t>& words);
void add_verifier(std::vector<std::uint32_t>& words);

} // namespace omni::devices::gameshark
