#pragma once

#include <array>
#include <cstdint>

namespace omni::devices::armax::tables {

extern const std::array<std::uint8_t, 56> generation_permutation;
extern const std::array<std::uint8_t, 8> generation_masks;
extern const std::array<std::uint8_t, 16> generation_rotations;
extern const std::array<std::uint8_t, 48> generation_selection;
extern const std::array<std::uint16_t, 16> crc_high;
extern const std::array<std::uint16_t, 16> crc_low;
extern const std::array<std::uint8_t, 8> generation_seed;
extern const std::array<std::array<std::uint32_t, 64>, 8> substitution;

} // namespace omni::devices::armax::tables
