#pragma once

#include <array>
#include <cstdint>

namespace omni::devices::codebreaker::cb7_tables {

using SeedRow = std::array<std::uint8_t, 256>;
using SeedTable = std::array<SeedRow, 5>;
using Key = std::array<std::uint32_t, 5>;

extern const SeedTable default_seeds;
extern const Key default_key;

} // namespace omni::devices::codebreaker::cb7_tables
