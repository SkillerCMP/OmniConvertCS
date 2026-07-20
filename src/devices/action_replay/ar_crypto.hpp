#pragma once

#include <array>
#include <cstdint>

namespace omni::devices::action_replay {

inline constexpr std::uint32_t ar1_seed = 0x05100518U;
inline constexpr std::uint32_t ar2_key_address = 0xDEADFACEU;
inline constexpr std::uint32_t ar2_raw_enable_address = 0x0E3C7DF2U;
inline constexpr std::uint32_t ar2_raw_enable_value = 0x1456E7A5U;
inline constexpr std::uint8_t ar2_key_type = 0x05U;
inline constexpr std::uint8_t ar2_key_seed = 0x18U;

[[nodiscard]] std::uint32_t decrypt_word(std::uint32_t code,
                                         std::uint8_t type,
                                         std::uint8_t seed) noexcept;

[[nodiscard]] std::uint32_t encrypt_word(std::uint32_t code,
                                         std::uint8_t type,
                                         std::uint8_t seed) noexcept;

[[nodiscard]] std::array<std::uint8_t, 4> seed_bytes(std::uint32_t key) noexcept;
[[nodiscard]] std::uint32_t displayed_key_from_seed(std::uint32_t seed) noexcept;
[[nodiscard]] std::uint32_t seed_from_displayed_key(std::uint32_t key) noexcept;
[[nodiscard]] bool is_raw_enable_seed(std::uint32_t seed) noexcept;
[[nodiscard]] std::uint32_t seed_key(const std::array<std::uint8_t, 4>& bytes) noexcept;

} // namespace omni::devices::action_replay
